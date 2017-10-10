#include "swift/WALASupport/InstrKindInfoGetter.h"
#include "swift/Demangling/Demangle.h"
#include <string>

using namespace swift;
using std::string;

InstrKindInfoGetter::InstrKindInfoGetter(SILInstruction* instr, WALAIntegration* wala, raw_ostream* outs) {
	this->instr = instr;
	this->wala = wala;
	this->outs = outs;
}

void InstrKindInfoGetter::handleApplyInst() {
	// ValueKind indentifier
	if (outs != NULL) {
		*outs << "<< ApplyInst >>" << "\n";
	}

	// Cast the instr 
	ApplyInst *castInst = cast<ApplyInst>(instr);

	// Iterate args and output SILValue (to check argument types)
	string funcName = Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName());
	if (outs != NULL) {
		*outs << "\t [funcName] " << funcName << "\n";
	}

	// if (funcName indicates that castInst is an built-in operator)
	//    create a built-in operator node for it
	// else
	//    create a function calling node for it

	// there are 2 possibilities: 
	//   1. the function is a built-in operator in WALA
	//   2. the function is not a built-in operator in WALA
	for (unsigned i = 0; i < castInst->getNumArguments(); ++i) {
		SILValue v = castInst->getArgument(i);
		// ValueKind argumentKind = v->getKind();
		if (outs != NULL) {
			*outs << "\t [ARG] #" << i << ": " << v << "\n";
		}
	}
}

void InstrKindInfoGetter::handleStringLiteralInst() {
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
	jobject walaConstant = wala->makeConstant(value);
}

void InstrKindInfoGetter::handleConstStringLiteralInst() {
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
	jobject walaConstant = wala->makeConstant(value);
}

void InstrKindInfoGetter::handleFunctionRefInst() {
	// ValueKind identifier
	if (outs != NULL) {
		*outs << "<< FunctionRefInst >>" << "\n";
	}

	// Cast the instr to access methods
	FunctionRefInst *castInst = cast<FunctionRefInst>(instr);

	// Demangled FunctionRef name
	if (outs != NULL) {
		*outs << "=== [FUNC] Ref'd: ";
	}
	
	string funcName = Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName());
	if (outs != NULL) {
		*outs << funcName << "\n";
	}
}

ValueKind InstrKindInfoGetter::get() {
	auto instrKind = instr->getKind();

	switch (instrKind) {
	
		case ValueKind::SILPHIArgument:
		case ValueKind::SILFunctionArgument:
		case ValueKind::SILUndef: {		
// 			outfile		<< "\t << Not an instruction >>" << "\n";
			break;
		}
		
		case ValueKind::AllocBoxInst: {
// 			outfile		<< "\t << AllocBoxInst >>" << "\n";
			break;
		}
	
		case ValueKind::ApplyInst: {
			handleApplyInst();
			break;
		}
		
		case ValueKind::PartialApplyInst: {
// 			outfile		<< "\t << PartialApplyInst >>" << "\n";
			break;
		}
		
		case ValueKind::IntegerLiteralInst: {
// 			outfile		<< "\t << IntegerLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::FloatLiteralInst: {
// 			outfile		<< "\t << FloatLiteralInst >>" << "\n";
			break;
		}
		
		case ValueKind::StringLiteralInst: {
			handleStringLiteralInst();
			break;
		}
		
		case ValueKind::ConstStringLiteralInst: {
			handleConstStringLiteralInst();
			break;
		}
		
		case ValueKind::AllocValueBufferInst: {
// 			outfile		<< "\t << AllocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectValueBufferInst: {
// 			outfile		<< "\t << ProjectValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocValueBufferInst: {
// 			outfile		<< "\t << DeallocValueBufferInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBoxInst: {
// 			outfile		<< "\t << ProjectBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectExistentialBoxInst: {
// 			outfile		<< "\t << ProjectExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::FunctionRefInst: {
			handleFunctionRefInst();

			break;
		}
		
		case ValueKind::BuiltinInst: {
// 			outfile		<< "\t << BuiltinInst >>" << "\n";
			break;
		}
		
		case ValueKind::OpenExistentialAddrInst:
		case ValueKind::OpenExistentialBoxInst:
		case ValueKind::OpenExistentialBoxValueInst:
		case ValueKind::OpenExistentialMetatypeInst:
		case ValueKind::OpenExistentialRefInst:
		case ValueKind::OpenExistentialValueInst: {
// 			outfile		<< "\t << OpenExistential[Addr/Box/BoxValue/Metatype/Ref/Value]Inst >>" << "\n";
			break;
		}
		
		// UNARY_INSTRUCTION(ID) <see ParseSIL.cpp:2248>
		// DEFCOUNTING_INSTRUCTION(ID) <see ParseSIL.cpp:2255>
		
		case ValueKind::DebugValueInst: {
// 			outfile		<< "\t << DebugValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DebugValueAddrInst: {
// 			outfile		<< "\t << DebugValueAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::UncheckedOwnershipConversionInst: {
// 			outfile		<< "\t << UncheckedOwnershipConversionInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadInst: {
// 			outfile		<< "\t << LoadInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadBorrowInst: {
// 			outfile		<< "\t << LoadBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginBorrowInst: {
// 			outfile		<< "\t << BeginBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadUnownedInst: {
// 			outfile		<< "\t << LoadUnownedInst >>" << "\n";
			break;
		}
		
		case ValueKind::LoadWeakInst: {
// 			outfile		<< "\t << LoadWeakInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkDependenceInst: {
// 			outfile		<< "\t << MarkDependenceInst >>" << "\n";
			break;
		}
		
		case ValueKind::KeyPathInst: {
// 			outfile		<< "\t << KeyPathInst >>" << "\n";
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
// 			outfile		<< "\t << Conversion Instruction >>" << "\n";
  			break;
  		}
  		
  		case ValueKind::PointerToAddressInst: {
// 			outfile		<< "\t << PointerToAddressInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefToBridgeObjectInst: {
// 			outfile		<< "\t << RefToBridgeObjectInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastAddrInst:
		case ValueKind::CheckedCastAddrBranchInst:
		case ValueKind::UncheckedRefCastAddrInst: {
// 			outfile		<< "\t << Indirect checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastValueInst: {
// 			outfile		<< "\t << UnconditionalCheckedCastValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnconditionalCheckedCastInst:
		case ValueKind::CheckedCastValueBranchInst:
		case ValueKind::CheckedCastBranchInst: {
// 			outfile		<< "\t << Checked conversion instruction >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedInst: {
// 			outfile		<< "\t << MarkUninitializedInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkUninitializedBehaviorInst: {
// 			outfile		<< "\t << MarkUninitializedBehaviorInst >>" << "\n";
			break;
		}
		
		case ValueKind::MarkFunctionEscapeInst: {
// 			outfile		<< "\t << MarkFunctionEscapeInst >>" << "\n";
			break;
		}
		
		case ValueKind::StoreInst: {
// 			outfile		<< "\t << StoreInst >>" << "\n";
			break;
		}
		
		case ValueKind::EndBorrowInst: {
// 			outfile		<< "\t << EndBorrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BeginAccessInst:
		case ValueKind::BeginUnpairedAccessInst:
		case ValueKind::EndAccessInst:
		case ValueKind::EndUnpairedAccessInst: {
// 			outfile		<< "\t << Access Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::StoreBorrowInst:
		case ValueKind::AssignInst:
		case ValueKind::StoreUnownedInst:
		case ValueKind::StoreWeakInst: {
// 			outfile		<< "\t << Access Instruction >>" << "\n";
			break;
		}

		case ValueKind::AllocStackInst: {
// 			outfile		<< "\t << AllocStack Instruction >>" << "\n";
			break;
		}
		case ValueKind::MetatypeInst: {		
// 			outfile		<< "\t << MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocRefInst:
		case ValueKind::AllocRefDynamicInst: {
// 			outfile		<< "\t << Alloc[Ref/RefDynamic] Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocStackInst: {		
// 			outfile		<< "\t << DeallocStackInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocRefInst: {		
// 			outfile		<< "\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocPartialRefInst: {		
// 			outfile		<< "\t << DeallocPartialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocBoxInst: {		
// 			outfile		<< "\t << DeallocBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::ValueMetatypeInst: 
		case ValueKind::ExistentialMetatypeInst: {		
// 			outfile		<< "\t << [Value/Existential]MetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeallocExistentialBoxInst: {		
// 			outfile		<< "\t << DeallocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleInst: {		
// 			outfile		<< "\t << TupleInst >>" << "\n";
			break;
		}
		
		case ValueKind::EnumInst: {		
// 			outfile		<< "\t << EnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitEnumDataAddrInst:
		case ValueKind::UncheckedEnumDataInst:
		case ValueKind::UncheckedTakeEnumDataAddrInst: {		
// 			outfile		<< "\t << EnumData Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::InjectEnumAddrInst: {		
// 			outfile		<< "\t << InjectEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TupleElementAddrInst:
		case ValueKind::TupleExtractInst: {		
// 			outfile		<< "\t << Tuple Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::ReturnInst: {		
// 			outfile		<< "\t << ReturnInst >>" << "\n";
			break;
		}
		
		case ValueKind::ThrowInst: {		
// 			outfile		<< "\t << ThrowInst >>" << "\n";
			break;
		}
		
		case ValueKind::BranchInst: {		
// 			outfile		<< "\t << BranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::CondBranchInst: {		
// 			outfile		<< "\t << CondBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::UnreachableInst: {		
// 			outfile		<< "\t << UnreachableInst >>" << "\n";
			break;
		}
		
		case ValueKind::ClassMethodInst:
		case ValueKind::SuperMethodInst:
		case ValueKind::DynamicMethodInst: {		
// 			outfile		<< "\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::WitnessMethodInst: {		
// 			outfile		<< "\t << WitnessMethodInst >>" << "\n";
			break;
		}
		
		case ValueKind::CopyAddrInst: {		
// 			outfile		<< "\t << CopyAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::BindMemoryInst: {		
// 			outfile		<< "\t << BindMemoryInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructInst: {		
// 			outfile		<< "\t << StructInst >>" << "\n";
			break;
		}
		
		case ValueKind::StructElementAddrInst:
		case ValueKind::StructExtractInst: {		
// 			outfile		<< "\t << Struct Instruction >>" << "\n";
			break;
		}
		
		case ValueKind::RefElementAddrInst: {		
// 			outfile		<< "\t << RefElementAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::RefTailAddrInst: {		
// 			outfile		<< "\t << RefTailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IsNonnullInst: {		
// 			outfile		<< "\t << IsNonnullInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexAddrInst: {		
// 			outfile		<< "\t << IndexAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::TailAddrInst: {		
// 			outfile		<< "\t << TailAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::IndexRawPointerInst: {		
// 			outfile		<< "\t << IndexRawPointerInst >>" << "\n";
			break;
		}
		
		case ValueKind::ObjCProtocolInst: {		
// 			outfile		<< "\t << ObjCProtocolInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocGlobalInst: {		
// 			outfile		<< "\t << AllocGlobalInst >>" << "\n";
			break;
		}
		
		case ValueKind::GlobalAddrInst: {		
// 			outfile		<< "\t << GlobalAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumInst: {		
// 			outfile		<< "\t << SelectEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectEnumAddrInst: {		
// 			outfile		<< "\t << DeallocRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumInst: {		
// 			outfile		<< "\t << SwitchEnumInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchEnumAddrInst: {		
// 			outfile		<< "\t << SwitchEnumAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::SwitchValueInst: {		
// 			outfile		<< "\t << SwitchValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::SelectValueInst: {		
// 			outfile		<< "\t << SelectValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialAddrInst: {		
// 			outfile		<< "\t << DeinitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::DeinitExistentialValueInst: {		
// 			outfile		<< "\t << DeinitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialAddrInst: {		
// 			outfile		<< "\t << InitExistentialAddrInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialValueInst: {		
// 			outfile		<< "\t << InitExistentialValueInst >>" << "\n";
			break;
		}
		
		case ValueKind::AllocExistentialBoxInst: {		
// 			outfile		<< "\t << AllocExistentialBoxInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialRefInst: {		
// 			outfile		<< "\t << InitExistentialRefInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitExistentialMetatypeInst: {		
// 			outfile		<< "\t << InitExistentialMetatypeInst >>" << "\n";
			break;
		}
		
		case ValueKind::DynamicMethodBranchInst: {		
// 			outfile		<< "\t << DynamicMethodBranchInst >>" << "\n";
			break;
		}
		
		case ValueKind::ProjectBlockStorageInst: {		
// 			outfile		<< "\t << ProjectBlockStorageInst >>" << "\n";
			break;
		}
		
		case ValueKind::InitBlockStorageHeaderInst: {		
// 			outfile		<< "\t << InitBlockStorageHeaderInst >>" << "\n";
			break;
		}		
		
		default: {
// 			outfile 	<< "\t\t xxxxx Not a handled inst type \n";
			break;
		}
	}

	return instrKind;
}