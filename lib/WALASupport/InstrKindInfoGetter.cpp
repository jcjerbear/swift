#include "swift/WALASupport/InstrKindInfoGetter.h"
#include "swift/Demangling/Demangle.h"
#include "swift/AST/ASTNode.h"
#include "CAstWrapper.h"
#include <string>
#include <list>
#include <cassert>
#include <algorithm>

using namespace swift;
using std::string;
using std::list;

InstrKindInfoGetter::InstrKindInfoGetter(SILInstruction* instr, WALAIntegration* wala, 
										unordered_map<void*, jobject>* nodeMap, list<jobject>* nodeList, 
										BasicBlockLabeller* labeller,
										raw_ostream* outs) {
	this->instr = instr;
	this->wala = wala;
	this->nodeMap = nodeMap;
	this->nodeList = nodeList; // top level CAst nodes only
	this->labeller = labeller;
	this->outs = outs;
	*outs << "HELOOOoooooooooooooooooooooooooooooooooooo$$$$$$$$\n";
	std::cout<< "YOU SUCK"<<std::endl;
	std::cout<< "Did this."<<std::endl;

	*outs<<"12345\n";

}

bool InstrKindInfoGetter::isBuiltInFunction(SILFunction* function) {
	return Demangle::demangleSymbolAsString(function->getName()) == "static Swift.String.+ infix(Swift.String, Swift.String) -> Swift.String";
}

jobject InstrKindInfoGetter::handleApplyInst() {
	// ValueKind indentifier
	if (outs != NULL) {
		*outs << "<< ApplyInst >>" << "\n";
	}

	jobject node = nullptr; // the CAst node to be created

	// Cast the instr 
	ApplyInst *castInst = cast<ApplyInst>(instr);
	SILFunction* function = castInst->getReferencedFunction();
	SILLocation location = function->getLocation();
	Decl* ASTnode = location.getAsASTNode<Decl>();
	*outs << "ASTnode: " << ASTnode << "\n";
	*outs << "isUnaryOperator(): " << ((FuncDecl*)ASTnode)->isUnaryOperator() << "\n";
	*outs << "isBinaryOperator(): " << ((FuncDecl*)ASTnode)->isBinaryOperator() << "\n";

	if (isBuiltInFunction(castInst->getReferencedFunction())) {
		if (outs != NULL) {
			*outs << "\tThis is an built-in operator\n";
		}
		if (Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName()) == "static Swift.String.+ infix(Swift.String, Swift.String) -> Swift.String") {
			jobject firstOperand = nullptr;
			jobject secondOperand = nullptr;
			if (nodeMap->find(castInst->getArgument(0).getOpaqueValue()) != nodeMap->end()) {
				firstOperand= nodeMap->at(castInst->getArgument(0).getOpaqueValue());
			}
			*outs << "\t [ARG] #1" << ": " << castInst->getArgument(0);
			*outs << "\t [ADDR] #1" << ": " << castInst->getArgument(0).getOpaqueValue() << "\n";
			if (nodeMap->find(castInst->getArgument(1).getOpaqueValue()) != nodeMap->end()) {
				secondOperand= nodeMap->at(castInst->getArgument(1).getOpaqueValue());
			}
			*outs << "\t [ARG] #2" << ": " << castInst->getArgument(1);
			*outs << "\t [ADDR] #2" << ": " << castInst->getArgument(1).getOpaqueValue() << "\n";
			node = (*wala)->makeNode(CAstWrapper::BINARY_EXPR, CAstWrapper::OP_ADD, firstOperand, secondOperand);

			auto firstOperandIterator = std::find(nodeList->begin(), nodeList->end(), firstOperand);
			if (firstOperandIterator != nodeList->end()) {
				nodeList->erase(firstOperandIterator);
			}

			auto secondOperandIterator = std::find(nodeList->begin(), nodeList->end(), secondOperand);
			if (secondOperandIterator != nodeList->end()) {
				nodeList->erase(secondOperandIterator);
			}
		}
	} else {
		if (outs != NULL) {
			*outs << "\t [CALLEE]: " << Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName()) << "\n";
		}

		if (nodeMap->find(castInst->getReferencedFunction()) == nodeMap->end()) {
			if (outs != NULL) {
				*outs << "something terribly wrong happens: failed to find the CAst node for the callee\n";
			}
		}
		jobject funcExprNode = nodeMap->at(castInst->getReferencedFunction());

		list<jobject> params;
		for (unsigned i = 0; i < castInst->getNumArguments(); ++i) {
			SILValue v = castInst->getArgument(i);
			if (nodeMap->find(v.getOpaqueValue()) != nodeMap->end()) {
				jobject child = nodeMap->at(v.getOpaqueValue());
				params.push_back(child);

				auto paramIterator = std::find(nodeList->begin(), nodeList->end(), child);
				if (paramIterator != nodeList->end()) {
					nodeList->erase(paramIterator);
				}
			} else {
				// This should not happen in the end after we finish this class. We should have a CAst node in the map for each and every argument
			}

			if (outs != NULL) {
				*outs << "\t [ARG] #" << i << ": " << v;
				*outs << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
			}
		}
		node = (*wala)->makeNode(CAstWrapper::CALL, funcExprNode, (*wala)->makeArray(&params));
	}

	nodeMap->insert(std::make_pair(castInst, node)); // insert the node into the hash map
	return nullptr;
}

jobject InstrKindInfoGetter::handleStringLiteralInst() {
	// ValueKind indentifier
	
	if (outs != NULL) {
		*outs << "<< StringLiteralInst >>" << "\n";
	}

	// Cast the instr to access methods
	StringLiteralInst *castInst = cast<StringLiteralInst>(instr);

	// Value: the string data for the literal, in UTF-8.
	StringRef value = castInst->getValue();
	
	if (outs != NULL) {
		*outs << "\t [value] " << value << "\n";
	}

	// Encoding: the desired encoding of the text.
	string encoding;
	switch (castInst->getEncoding()) {
		case StringLiteralInst::Encoding::UTF8: {
			encoding = "UTF8";
			break;
		}
		case StringLiteralInst::Encoding::UTF16: {
			encoding = "UTF16";
			break;
		}
		case StringLiteralInst::Encoding::ObjCSelector: {
			encoding = "ObjCSelector";
			break;
		}
	}
	
	if (outs != NULL) {
		*outs << "\t [encoding] " << encoding << "\n";
	}

	// Count: encoding-based length of the string literal in code units.
	uint64_t codeUnitCount = castInst->getCodeUnitCount();
	
	if (outs != NULL) {
		*outs << "\t [codeUnitCount] " << codeUnitCount << "\n";
	}

	// Call WALA in Java
	jobject walaConstant = (*wala)->makeConstant(((string)value).c_str());

	nodeMap->insert(std::make_pair(castInst, walaConstant));

	return walaConstant;
}
jobject InstrKindInfoGetter::handleIntegerLiteralInst(){
	assert(outs != NULL);
	*outs << "<< IntegerLiteralInst >>\n";
	IntegerLiteralInst *castedInst = cast<IntegerLiteralInst>(instr);
	APInt val = castedInst->getValue();
	jobject walaConstant = nullptr;
	return walaConstant;
}



jobject InstrKindInfoGetter::handleConstStringLiteralInst() {
	// ValueKind indentifier
	if (outs != NULL) {
		*outs << "<< handleConstStringLiteralInst >>" << "\n";
	}

	// Cast the instr to access methods
	ConstStringLiteralInst *castInst = cast<ConstStringLiteralInst>(instr);

	// Value: the string data for the literal, in UTF-8.
	StringRef value = castInst->getValue();
	if (outs != NULL) {
		*outs << "\t [value] " << value << "\n";
	}

	// Encoding: the desired encoding of the text.
	string encoding;
	switch (castInst->getEncoding()) {
		case ConstStringLiteralInst::Encoding::UTF8: {
			encoding = "UTF8";
			break;
		}
		case ConstStringLiteralInst::Encoding::UTF16: {
			encoding = "UTF16";
			break;
		}
	}
	if (outs != NULL) {
		*outs << "\t [ENCODING]: " << encoding << "\n";
	}

	// Count: encoding-based length of the string literal in code units.
	uint64_t codeUnitCount = castInst->getCodeUnitCount();
	if (outs != NULL) {
		*outs << "\t [COUNT]: " << codeUnitCount << "\n";
	}

	// Call WALA in Java
	jobject walaConstant = (*wala)->makeConstant(((string)value).c_str());

	nodeMap->insert(std::make_pair(castInst, walaConstant));

	return walaConstant;
}

jobject InstrKindInfoGetter::handleFunctionRefInst() {
	// ValueKind identifier
	if (outs != NULL) {
		*outs << "<< FunctionRefInst >>" << "\n";
	}

	// Cast the instr to access methods
	FunctionRefInst *castInst = cast<FunctionRefInst>(instr);
	
	string funcName = Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName());
	jobject nameNode = (*wala)->makeConstant(funcName.c_str());
	jobject funcExprNode = (*wala)->makeNode(CAstWrapper::FUNCTION_EXPR, nameNode);

	if (outs != NULL) {
		*outs << "=== [FUNC] Ref'd: ";
		*outs << funcName << "\n";
	}

	nodeMap->insert(std::make_pair(castInst->getReferencedFunction(), funcExprNode));

	return nullptr;
}

jobject InstrKindInfoGetter::handleStoreInst() {
	// ValueKind identifier
	if (outs != NULL) {
		*outs << "<< StoreInst >>" << "\n";
	}

	// Cast the instr to access methods
	StoreInst *castInst = cast<StoreInst>(instr);
	SILValue src = castInst->getSrc();
	SILValue dest = castInst->getDest();
	if (outs != NULL) {
		*outs << "\t [SRC]: " << src.getOpaqueValue() << "\n";
		*outs << "\t [DEST]: " << dest.getOpaqueValue() << "\n";
	}

	//nodeMap->insert(std::make_pair(castInst->getReferencedFunction(), funcExprNode));

	return nullptr;
}

jobject InstrKindInfoGetter::handleBranchInst() {
	// ValueKind identifier
	if (outs != NULL) {
		*outs << "<< BranchInst >>" << "\n";
	}

	// Cast the instr to access methods
	BranchInst *castInst = cast<BranchInst>(instr);

	// This is an unconditional branch
	jobject gotoNode = nullptr;

	// Destination block
	int i = 0;
	SILBasicBlock* destBasicBlock = castInst->getDestBB();
	if (outs != NULL) {
		*outs << "\t [DESTBB]: " << destBasicBlock << "\n";
		if (destBasicBlock != NULL) {
			for (auto& instr : *destBasicBlock) {
				*outs << "\t\t [INST" << i++ << "]: " << &instr << "\n";
			}
		}
	}
	if (destBasicBlock != NULL) {
		jobject labelNode = (*wala)->makeConstant(labeller->label(destBasicBlock).c_str());
		gotoNode = (*wala)->makeNode(CAstWrapper::GOTO, labelNode);
	}

	return gotoNode;
}

jobject InstrKindInfoGetter::handleCondBranchInst() {
	// ValueKind identifier
	if (outs != NULL) {
		*outs << "<< CondBranchInst >>" << "\n";
	}

	// Cast the instr to access methods
	CondBranchInst *castInst = cast<CondBranchInst>(instr);

	// 1. Condition
	SILValue cond = castInst->getCondition();
	jobject condNode = nullptr;
	if (nodeMap->find(cond.getOpaqueValue()) != nodeMap->end()) {
		condNode = nodeMap->at(cond.getOpaqueValue());

		auto condIterator = std::find(nodeList->begin(), nodeList->end(), condNode);
		if (condIterator != nodeList->end()) {
			nodeList->erase(condIterator);
		}
	}
	if (outs != NULL) {
		*outs << "\t [COND]: " << cond.getOpaqueValue() << "\n";
	}

	// 2. True block
	int i = 0;
	SILBasicBlock* trueBasicBlock = castInst->getTrueBB();
	jobject trueGotoNode = nullptr;
	if (outs != NULL) {
		*outs << "\t [TBB]: " << trueBasicBlock << "\n";
		if (trueBasicBlock != NULL) {
			for (auto& instr : *trueBasicBlock) {
				*outs << "\t\t [INST" << i++ << "]: " << &instr << "\n";
			}
		}
	}
	if (trueBasicBlock != NULL) {
		jobject labelNode = (*wala)->makeConstant(labeller->label(trueBasicBlock).c_str());
		trueGotoNode = (*wala)->makeNode(CAstWrapper::GOTO, labelNode);
	}

	// 3. False block
	i = 0;
	SILBasicBlock* falseBasicBlock = castInst->getFalseBB();
	jobject falseGotoNode = nullptr;
	if (outs != NULL) {
		*outs << "\t [FBB]: " << falseBasicBlock << "\n";
		if (falseBasicBlock != NULL) {
			for (auto& instr : *falseBasicBlock) {
				*outs << "\t\t [INST" << i++ << "]: " << &instr << "\n";
			}
		}
	}
	if (falseBasicBlock != NULL) {
		jobject labelNode = (*wala)->makeConstant(labeller->label(falseBasicBlock).c_str());
		falseGotoNode = (*wala)->makeNode(CAstWrapper::GOTO, labelNode);
	}

	// 4. Assemble them into an if-stmt node
	jobject ifStmtNode = nullptr;
	if (falseGotoNode != nullptr) { // with else block
		ifStmtNode = (*wala)->makeNode(CAstWrapper::IF_STMT, condNode, trueGotoNode, falseGotoNode);
	} else { // without else block
		ifStmtNode = (*wala)->makeNode(CAstWrapper::IF_STMT, condNode, trueGotoNode);
	}

	nodeMap->insert(std::make_pair(castInst, ifStmtNode));

	return ifStmtNode;
}


jobject InstrKindInfoGetter::handleAssignInst(){
	*outs << "<<Assign Instruction>>" << "\n";
	AssignInst *castInst = cast<AssignInst>(instr);
	*outs << "[source]:" << castInst->getSrc().getOpaqueValue() << "\n";
	*outs << "[Dest]:" << castInst->getDest().getOpaqueValue() << "\n";
	jobject dest = nullptr;
	jobject expr = nullptr;
	if (nodeMap->find(castInst->getDest().getOpaqueValue()) != nodeMap->end()) {
		dest = nodeMap->at(castInst->getDest().getOpaqueValue());
	}
	if(dest == nullptr)
		*outs << "null dest!\n";
	if (nodeMap->find(castInst->getSrc().getOpaqueValue()) != nodeMap->end()) {
		expr = nodeMap->at(castInst->getSrc().getOpaqueValue());
	}
	if(expr == nullptr)
		*outs << "null expr!\n";
	jobject assign_node = (*wala)->makeNode(CAstWrapper::ASSIGN,dest,expr);
	nodeMap->insert(std::make_pair(castInst,assign_node));
	return assign_node;
}

ValueKind InstrKindInfoGetter::get() {
	auto instrKind = instr->getKind();
	jobject node = nullptr;

	switch (instrKind) {
	
		case ValueKind::SILPHIArgument:
		case ValueKind::SILFunctionArgument:
		case ValueKind::SILUndef: {		
			*outs << "<< Not an instruction >>" << "\n";
			break;
		}
		
		case ValueKind::AllocBoxInst: {
			*outs << "<< AllocBoxInst >>" << "\n";
			break;
		}
	
		case ValueKind::ApplyInst: {
			//node = handleApplyInst();
			handleApplyInst();
			break;
		}
		
		case ValueKind::PartialApplyInst: {
			*outs << "<< PartialApplyInst >>" << "\n";
			break;
		}
		
		case ValueKind::IntegerLiteralInst: {

			*outs << "<< IntegerLiteralInst >>" << "\n";

			IntegerLiteralInst* castInst = cast<IntegerLiteralInst>(instr);
			APInt value = castInst->getValue();
			node = (*wala)->makeConstant((int)value.getSExtValue());
			nodeMap->insert(std::make_pair(castInst, node));
			break;
		}
		
		case ValueKind::FloatLiteralInst: {
			*outs << "<< FloatLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::StringLiteralInst: {
			node = handleStringLiteralInst();
			break;
		}
		
		case ValueKind::ConstStringLiteralInst: {
			node = handleConstStringLiteralInst();
			break;
		}
		
		case ValueKind::AllocValueBufferInst: {
			*outs << "<< AllocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectValueBufferInst: {
			*outs << "<< ProjectValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocValueBufferInst: {
			*outs << "<< DeallocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBoxInst: {
			*outs << "<< ProjectBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectExistentialBoxInst: {
			*outs << "<< ProjectExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::FunctionRefInst: {
			node = handleFunctionRefInst();
			break;
		}
		
		case ValueKind::BuiltinInst: {
			*outs << "<< BuiltinInst >>" << "\n";
			break;
		}
		
		case ValueKind::OpenExistentialAddrInst:
		case ValueKind::OpenExistentialBoxInst:
		case ValueKind::OpenExistentialBoxValueInst:
		case ValueKind::OpenExistentialMetatypeInst:
		case ValueKind::OpenExistentialRefInst:
		case ValueKind::OpenExistentialValueInst: {
			*outs << "<< OpenExistential[Addr/Box/BoxValue/Metatype/Ref/Value]Inst >>" << "\n";
			break;
		}
		
		// UNARY_INSTRUCTION(ID) <see ParseSIL.cpp:2248>
		// DEFCOUNTING_INSTRUCTION(ID) <see ParseSIL.cpp:2255>
		
		case ValueKind::DebugValueInst: {
			*outs << "<< DebugValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DebugValueAddrInst: {
			*outs << "<< DebugValueAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedOwnershipConversionInst: {
			*outs << "<< UncheckedOwnershipConversionInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadInst: {
			*outs << "<< LoadInst >>" << "\n";
			LoadInst *castInst = cast<LoadInst>(instr);
			*outs << "\t\t [name]:" << (castInst->getOperand()).getOpaqueValue() << "\n";
			jobject node = nullptr;
			if (nodeMap->find(castInst->getOperand().getOpaqueValue()) != nodeMap->end()) {
				node = nodeMap->at(castInst->getOperand().getOpaqueValue());
			}
			nodeMap->insert(std::make_pair(castInst, node));
			break;
		}
		
		case ValueKind::LoadBorrowInst: {
			*outs << "<< LoadBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginBorrowInst: {
			*outs << "<< BeginBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadUnownedInst: {
			*outs << "<< LoadUnownedInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadWeakInst: {
			*outs << "<< LoadWeakInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkDependenceInst: {
			*outs << "<< MarkDependenceInst >>" << "\n";
			break;
		}
		
		case ValueKind::KeyPathInst: {
			*outs << "<< KeyPathInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedRefCastInst:
		case ValueKind::UncheckedAddrCastInst:
		case ValueKind::UncheckedTrivialBitCastInst:
		case ValueKind::UncheckedBitwiseCastInst:
		case ValueKind::UpcastInst:
		case ValueKind::AddressToPointerInst:
		case ValueKind::BridgeObjectToRefInst:
		case ValueKind::BridgeObjectToWordInst:
		case ValueKind::RefToRawPointerInst:
		case ValueKind::RawPointerToRefInst:
		case ValueKind::RefToUnownedInst:
		case ValueKind::UnownedToRefInst:
		case ValueKind::RefToUnmanagedInst:
		case ValueKind::UnmanagedToRefInst:
		case ValueKind::ThinFunctionToPointerInst:
		case ValueKind::PointerToThinFunctionInst:
		case ValueKind::ThinToThickFunctionInst:
		case ValueKind::ThickToObjCMetatypeInst:
		case ValueKind::ObjCToThickMetatypeInst:
		case ValueKind::ConvertFunctionInst:
		case ValueKind::ObjCExistentialMetatypeToObjectInst:
		case ValueKind::ObjCMetatypeToObjectInst: {
			*outs << "<< Conversion Instruction >>" << "\n";
  			break;
  		}
  		
  		case ValueKind::PointerToAddressInst: {
			*outs << "<< PointerToAddressInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefToBridgeObjectInst: {
			*outs << "<< RefToBridgeObjectInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastAddrInst:
		case ValueKind::CheckedCastAddrBranchInst:
		case ValueKind::UncheckedRefCastAddrInst: {
			*outs << "<< Indirect checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastValueInst: {
			*outs << "<< UnconditionalCheckedCastValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastInst:
		case ValueKind::CheckedCastValueBranchInst:
		case ValueKind::CheckedCastBranchInst: {
			*outs << "<< Checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedInst: {
			*outs << "<< MarkUninitializedInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedBehaviorInst: {
			*outs << "<< MarkUninitializedBehaviorInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkFunctionEscapeInst: {
			*outs << "<< MarkFunctionEscapeInst >>" << "\n";
			break;
		}
		
		case ValueKind::StoreInst: {
			node = handleStoreInst();
			break;
		}
		
		case ValueKind::EndBorrowInst: {
			*outs << "<< EndBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginAccessInst:{
			*outs << "<< Begin Access >>" << "\n";
			BeginAccessInst *castInst = cast<BeginAccessInst>(instr);
			*outs << "\t\t [oper_addr]:" << (castInst->getOperand()).getOpaqueValue() << "\n";
			GlobalAddrInst *Global_var = (GlobalAddrInst *)(castInst->getOperand()).getOpaqueValue();
			SILGlobalVariable* variable = Global_var->getReferencedGlobal();
			StringRef var_name = variable->getName();
			*outs << "\t\t[Var name]:" << var_name << "\n";
			//*outs << ((string)var_name).c_str() << "\n";
			//*outs << "\t\t[Addr]:" << init_inst << "\n";
			jobject symbol = (*wala)->makeConstant(var_name.data());		
			//jobject node = nullptr;
			//
			//if (nodeMap->find((castInst->getOperand()).getOpaqueValue()) != nodeMap->end()) {
			//	node = nodeMap->at((castInst->getOperand()).getOpaqueValue());
			//}
			jobject read_var = (*wala)->makeNode(CAstWrapper::VAR,symbol);
			nodeMap->insert(std::make_pair(castInst, read_var));
			break;
		}
		case ValueKind::BeginUnpairedAccessInst:{
			*outs << "<<Begin Unpaired Access>>" << "\n";
			break;
		}
		case ValueKind::EndAccessInst:{
			*outs << "<< End Access >>" << "\n";
			break;
		}
		case ValueKind::EndUnpairedAccessInst: {
			*outs << "<< End Unpaired Access >>" << "\n";			
			break;
		}
		
		case ValueKind::StoreBorrowInst:
		case ValueKind::AssignInst:{
			node = handleAssignInst();
			break;
		}
		case ValueKind::StoreUnownedInst:
		case ValueKind::StoreWeakInst: {
			*outs << "<< Access Instruction >>" << "\n";
			break;
		}

		case ValueKind::AllocStackInst: {
			*outs << "<< AllocStack Instruction >>" << "\n";
			break;
		}
		case ValueKind::MetatypeInst: {		
			*outs << "<< MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocRefInst:
		case ValueKind::AllocRefDynamicInst: {
			*outs << "<< Alloc[Ref/RefDynamic] Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocStackInst: {		
			*outs << "<< DeallocStackInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocRefInst: {		
			*outs << "<< DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocPartialRefInst: {		
			*outs << "<< DeallocPartialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocBoxInst: {		
			*outs << "<< DeallocBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ValueMetatypeInst: 
		case ValueKind::ExistentialMetatypeInst: {		
			*outs << "<< [Value/Existential]MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocExistentialBoxInst: {		
			*outs << "<< DeallocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleInst: {		
			*outs << "<< TupleInst >>" << "\n";
			break;
		}
		
		case ValueKind::EnumInst: {		
			*outs << "<< EnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitEnumDataAddrInst:
		case ValueKind::UncheckedEnumDataInst:
		case ValueKind::UncheckedTakeEnumDataAddrInst: {		
			*outs << "<< EnumData Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::InjectEnumAddrInst: {		
			*outs << "<< InjectEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleElementAddrInst:
		case ValueKind::TupleExtractInst: {		
			*outs << "<< Tuple Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::ReturnInst: {		
			*outs << "<< ReturnInst >>" << "\n";
			break;
		}
		
		case ValueKind::ThrowInst: {		
			*outs << "<< ThrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BranchInst: {		
			node = handleBranchInst();
			break;
		}
		
		case ValueKind::CondBranchInst: {		
			node = handleCondBranchInst();
			break;
		}
		
		case ValueKind::UnreachableInst: {		
			*outs << "<< UnreachableInst >>" << "\n";
			break;
		}
		
		case ValueKind::ClassMethodInst:
		case ValueKind::SuperMethodInst:
		case ValueKind::DynamicMethodInst: {		
			*outs << "<< DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::WitnessMethodInst: {		
			*outs << "<< WitnessMethodInst >>" << "\n";
			break;
		}
		
		case ValueKind::CopyAddrInst: {		
			*outs << "<< CopyAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::BindMemoryInst: {		
			*outs << "<< BindMemoryInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructInst: {		
			*outs << "<< StructInst >>" << "\n";
			StructInst *castInst = cast<StructInst>(instr);

			break;
		}
		
		case ValueKind::StructElementAddrInst:
		case ValueKind::StructExtractInst: {		
			*outs << "<< Struct Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::RefElementAddrInst: {		
			*outs << "<< RefElementAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefTailAddrInst: {		
			*outs << "<< RefTailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IsNonnullInst: {		
			*outs << "<< IsNonnullInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexAddrInst: {		
			*outs << "<< IndexAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TailAddrInst: {		
			*outs << "<< TailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexRawPointerInst: {		
			*outs << "<< IndexRawPointerInst >>" << "\n";
			break;
		}
		
		case ValueKind::ObjCProtocolInst: {		
			*outs << "<< ObjCProtocolInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocGlobalInst: {		
			*outs << "<< AllocGlobalInst >>" << "\n";
			AllocGlobalInst *castInst = cast<AllocGlobalInst>(instr);
			SILGlobalVariable* variable = castInst->getReferencedGlobal();
			StringRef var_name = variable->getName();
			SILType var_type = variable->getLoweredType();
			*outs << "\t\t[Var name]:" << var_name << "\n";
			*outs << "\t\t[Var type]:" << var_type << "\n";
			break;
		}
		
		case ValueKind::GlobalAddrInst: {		
			*outs << "<< GlobalAddrInst >>" << "\n";
			GlobalAddrInst *castInst = cast<GlobalAddrInst>(instr);
			SILGlobalVariable* variable = castInst->getReferencedGlobal();
			StringRef var_name = variable->getName();
			*outs << "\t\t[Var name]:" << var_name << "\n";
			//*outs << ((string)var_name).c_str() << "\n";
			//*outs << "\t\t[Addr]:" << init_inst << "\n";
			jobject symbol = (*wala)->makeConstant(var_name.data());
			nodeMap->insert(std::make_pair(castInst, symbol));
			break;
		}
		
		case ValueKind::SelectEnumInst: {		
			*outs << "<< SelectEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumAddrInst: {		
			*outs << "<< DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumInst: {		
			*outs << "<< SwitchEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumAddrInst: {		
			*outs << "<< SwitchEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchValueInst: {		
			*outs << "<< SwitchValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectValueInst: {		
			*outs << "<< SelectValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialAddrInst: {		
			*outs << "<< DeinitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialValueInst: {		
			*outs << "<< DeinitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialAddrInst: {		
			*outs << "<< InitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialValueInst: {		
			*outs << "<< InitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocExistentialBoxInst: {		
			*outs << "<< AllocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialRefInst: {		
			*outs << "<< InitExistentialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialMetatypeInst: {		
			*outs << "<< InitExistentialMetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DynamicMethodBranchInst: {		
			*outs << "<< DynamicMethodBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBlockStorageInst: {		
			*outs << "<< ProjectBlockStorageInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitBlockStorageHeaderInst: {		
			*outs << "<< InitBlockStorageHeaderInst >>" << "\n";
			break;
		}		
		
		default: {
// 			outfile 	<< "\t\t xxxxx Not a handled inst type \n";
			break;
		}
	}

	if (node != nullptr) {
		nodeList->push_back(node);
		//wala->print(node);
	}
	return instrKind;
}