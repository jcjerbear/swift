#include "swift/WALASupport/InstrKindInfoGetter.h"
#include "swift/Demangling/Demangle.h"
#include "swift/AST/ASTNode.h"
#include "swift/AST/Identifier.h"
#include "CAstWrapper.h"
#include <string>
#include <list>
#include <algorithm>

using namespace swift;
using std::string;
using std::list;

InstrKindInfoGetter::InstrKindInfoGetter(SILInstruction* instr, WALAIntegration* wala, 
                    unordered_map<void*, jobject>* nodeMap, list<jobject>* nodeList, 
                    SymbolTable* symbolTable, BasicBlockLabeller* labeller,
                    raw_ostream* outs) {
  this->instr = instr;
  this->wala = wala;
  this->nodeMap = nodeMap;
  this->nodeList = nodeList; // top level CAst nodes only
  this->symbolTable = symbolTable;
  this->labeller = labeller;
  this->outs = outs;
}

// This function finds a CAst node in `symbolTable` using the key. The node will be removed from the nodeList
// If the key corresponds to a variable, a new VAR CAst node will be created and returned
// nullptr will be returned if such node does not exist
jobject InstrKindInfoGetter::findAndRemoveCAstNode(void* key) {
  jobject node = nullptr;
  if (symbolTable->has(key)) {
    // this is a variable
    jobject name = (*wala)->makeConstant(symbolTable->get(key).c_str());
    node = (*wala)->makeNode(CAstWrapper::VAR, name);
  } else if (nodeMap->find(key) != nodeMap->end()) {
    // find
    node = nodeMap->at(key);
    // remove
    auto it = std::find(nodeList->begin(), nodeList->end(), node);
    if (it != nodeList->end()) {
      nodeList->erase(it);
    }
  }

  return node;
}

bool InstrKindInfoGetter::isBuiltInFunction(SILFunction* function) {
  return isUnaryOperator(function) || isBinaryOperator(function);
}

bool InstrKindInfoGetter::isUnaryOperator(SILFunction* function) {
  SILLocation location = function->getLocation();
  Decl* ASTnode = location.getAsASTNode<Decl>();
  FuncDecl* funcDecl = static_cast<FuncDecl*>(ASTnode);
  return funcDecl->isUnaryOperator();
}

bool InstrKindInfoGetter::isBinaryOperator(SILFunction* function) {
  SILLocation location = function->getLocation();
  Decl* ASTnode = location.getAsASTNode<Decl>();
  FuncDecl* funcDecl = static_cast<FuncDecl*>(ASTnode);
  return funcDecl->isBinaryOperator();
}

Identifier InstrKindInfoGetter::getBuiltInOperatorName(SILFunction* function) {
  SILLocation location = function->getLocation();
  Decl* ASTnode = location.getAsASTNode<Decl>();
  FuncDecl* funcDecl = static_cast<FuncDecl*>(ASTnode);
  return funcDecl->getName();
}

jobject InstrKindInfoGetter::getOperatorCAstType(Identifier name) {
  if (name.is("==")) {
    return CAstWrapper::OP_EQ;
  } else if (name.is("!=")) {
    return CAstWrapper::OP_NE;
  } else if (name.is("+")) {
    return CAstWrapper::OP_ADD;
  } else if (name.is("/")) {
    return CAstWrapper::OP_DIV;
  } else if (name.is("<<")) {
    return CAstWrapper::OP_LSH;
  } else if (name.is("*")) {
    return CAstWrapper::OP_MUL;
  } else if (name.is(">>")) {
    return CAstWrapper::OP_RSH;
  } else if (name.is("-")) {
    return CAstWrapper::OP_SUB;
  } else if (name.is(">=")) {
    return CAstWrapper::OP_GE;
  } else if (name.is(">")) {
    return CAstWrapper::OP_GT;
  } else if (name.is("<=")) {
    return CAstWrapper::OP_LE;
  } else if (name.is("<")) {
    return CAstWrapper::OP_LT;
  } else if (name.is("!")) {
    return CAstWrapper::OP_NOT;
  } else if (name.is("~")) {
    return CAstWrapper::OP_BITNOT;
  } else if (name.is("&")) {
    return CAstWrapper::OP_BIT_AND;
  } else if (name.is("&&")) {
    // return CAstWrapper::OP_REL_AND;
    return nullptr; // && and || are handled separatedly because they involve short circuits
  } else if (name.is("|")) {
    return CAstWrapper::OP_BIT_OR;
  } else if (name.is("||")) {
    // return CAstWrapper::OP_REL_OR;
    return nullptr; // && and || are handled separatedly because they involve short circuits
  } else if (name.is("^")) {
    return CAstWrapper::OP_BIT_XOR;
  } else {
    return nullptr;
  }
}

jobject InstrKindInfoGetter::handleAllocBoxInst() {
  if (outs != NULL) {
    *outs << "<< AllocBoxInst >>" << "\n";
  }
  AllocBoxInst *castInst = cast<AllocBoxInst>(instr);

  SILDebugVariable info = castInst->getVarInfo();
  unsigned argNo = info.ArgNo;

  VarDecl *decl = castInst->getDecl();
  StringRef varName = decl->getNameStr();
  if (outs != NULL) {
    *outs << "[Arg]#" << argNo << ":" << varName << "\n";
  }
  symbolTable->insert(castInst, varName);
  return nullptr;
}

jobject InstrKindInfoGetter::handleApplyInst() {
  // ValueKind indentifier
  if (outs != NULL) {
    *outs << "<< ApplyInst >>" << "\n";
  }
  jobject node = nullptr; // the CAst node to be created
  // Cast the instr 
  ApplyInst *castInst = cast<ApplyInst>(instr);

  if (isBuiltInFunction(castInst->getReferencedFunction())) {
    Identifier name = getBuiltInOperatorName(castInst->getReferencedFunction());
    jobject operatorNode = getOperatorCAstType(name);
    if (operatorNode != nullptr) {
      if (outs != NULL) {
        *outs << "\t Built in operator\n";
        for (unsigned i = 0; i < castInst->getNumArguments(); ++i) {
          SILValue v = castInst->getArgument(i);
          *outs << "\t [ARG] #" << i << ": " << v;
          *outs << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
        }
      }
      if (isUnaryOperator(castInst->getReferencedFunction())) {
        // unary operator
        jobject operand = nullptr;
        if (castInst->getNumArguments() >= 2) {
          SILValue argument = castInst->getArgument(castInst->getNumArguments() - 2); // the second last one (last one is metatype)
          operand = findAndRemoveCAstNode(argument.getOpaqueValue());
        }
        node = (*wala)->makeNode(CAstWrapper::UNARY_EXPR, operatorNode, operand);
      } else {
        // binary operator
        jobject firstOperand = nullptr;
        jobject secondOperand = nullptr;
        if (castInst->getNumArguments() >= 3) {
          SILValue argument = castInst->getArgument(castInst->getNumArguments() - 3);
          firstOperand = findAndRemoveCAstNode(argument);
        }
        if (castInst->getNumArguments() >= 2) {
          SILValue argument = castInst->getArgument(castInst->getNumArguments() - 2);
          secondOperand = findAndRemoveCAstNode(argument);
        }
        node = (*wala)->makeNode(CAstWrapper::BINARY_EXPR, operatorNode, firstOperand, secondOperand);
      }
      nodeMap->insert(std::make_pair(castInst, node)); // insert the node into the hash map
      return node;
    } // otherwise, fall through to the regular funcion call logic
  } 
  if (outs != NULL) {
    *outs << "\t [CALLEE]: " << Demangle::demangleSymbolAsString(castInst->getReferencedFunction()->getName()) << "\n";
  }

  jobject funcExprNode = findAndRemoveCAstNode(castInst->getReferencedFunction());
  list<jobject> params;
  for (unsigned i = 0; i < castInst->getNumArguments(); ++i) {
    SILValue v = castInst->getArgument(i);
    jobject child = findAndRemoveCAstNode(v.getOpaqueValue());
    if (child != nullptr) {
      params.push_back(child);
    }
    if (outs != NULL) {
      *outs << "\t [ARG] #" << i << ": " << v;
      *outs << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
    }
  }
  node = (*wala)->makeNode(CAstWrapper::CALL, funcExprNode, (*wala)->makeArray(&params));
  nodeMap->insert(std::make_pair(castInst, node)); // insert the node into the hash map
  return node;
}

jobject InstrKindInfoGetter::handleIntegerLiteralInst() {
  if (outs != NULL) {
    *outs << "<< IntegerLiteralInst >>" << "\n";
  }
  IntegerLiteralInst* castInst = cast<IntegerLiteralInst>(instr);
  APInt value = castInst->getValue();
  jobject node = (*wala)->makeConstant((int)value.getSExtValue());
  nodeMap->insert(std::make_pair(castInst, node));
  return node;
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

jobject InstrKindInfoGetter::handleProjectBoxInst() {
  if (outs != NULL) {
    *outs << "<< ProjectBoxInst >>" << "\n";
  }
  ProjectBoxInst *castInst = cast<ProjectBoxInst>(instr);
  //*outs << "\t\t [addr]:" << castInst->getOperand().getOpaqueValue() << "\n";

  if (symbolTable->has(castInst->getOperand().getOpaqueValue())) {
    // this is a variable
    symbolTable->duplicate(castInst, symbolTable->get(castInst->getOperand().getOpaqueValue()).c_str());
  }
  return nullptr;
}

jobject InstrKindInfoGetter::handleDebugValueInst() {
  DebugValueInst *castInst = cast<DebugValueInst>(instr);

  SILDebugVariable info = castInst->getVarInfo();
  unsigned argNo = info.ArgNo;

  if (outs != NULL) {
    *outs << "<< DebugValueInst >>" << "\n";
    *outs << argNo << "\n";
  }

  VarDecl *decl = castInst->getDecl();
  if (decl != NULL) {
    string varName = decl->getNameStr();
    SILBasicBlock *parentBB = castInst->getParent();
    
    SILArgument *argument = NULL;

    if (argNo >= 1) {
      argument = parentBB->getArgument(argNo - 1);
      // variable declaration
      symbolTable->insert(argument, varName);
    }

    if (outs != NULL) {
      *outs << "\t\t[addr of arg]:" << argument << "\n";
    }
  }
  if (outs != NULL) {
    *outs << "\t\t[addr of arg]:" << castInst->getOperand().getOpaqueValue() << "\n";
  }

  return nullptr;
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
  nodeMap->insert(std::make_pair(castInst, funcExprNode));
  return nullptr;
}

jobject InstrKindInfoGetter::handleLoadInst() {
  if (outs != NULL) {
    *outs << "<< LoadInst >>" << "\n";
  }
  LoadInst *castInst = cast<LoadInst>(instr);
  if (outs != NULL) {
    *outs << "\t\t [name]:" << (castInst->getOperand()).getOpaqueValue() << "\n";
  }
  jobject node = findAndRemoveCAstNode((castInst->getOperand()).getOpaqueValue());

  nodeMap->insert(std::make_pair(castInst, node));
  return node;
}

jobject InstrKindInfoGetter::handleLoadBorrowInst() {
  if (outs != NULL) {
    *outs << "<< LoadBorrowInst >>" << "\n";
  }
  LoadBorrowInst *castInst = cast<LoadBorrowInst>(instr);
  if (outs != NULL) {
    *outs << "\t\t [name]:" << castInst->getOperand() << "\n";
    *outs << "\t\t [addr]:" << castInst->getOperand().getOpaqueValue() << "\n";
  }
  return nullptr;
}

jobject InstrKindInfoGetter::handleBeginBorrowInst() {
  if (outs != NULL) {
    *outs << "<< BeginBorrowInst >>" << "\n";
  }
  BeginBorrowInst *castInst = cast<BeginBorrowInst>(instr);
  //*outs << "\t\t [addr]:" << castInst->getOperand().getOpaqueValue() << "\n";

  return findAndRemoveCAstNode(castInst->getOperand().getOpaqueValue());
}

jobject InstrKindInfoGetter::handleThinToThickFunctionInst() {
  // ValueKind identifier
  if (outs != NULL) {
    *outs << "<< ThinToThickFunctionInst >>" << "\n";
  }

  // Cast the instr to access methods
  ThinToThickFunctionInst *castInst = cast<ThinToThickFunctionInst>(instr);

  if (outs != NULL) {
    *outs << "Callee: ";
    *outs << castInst->getCallee().getOpaqueValue() << "\n";
  }
  jobject funcRefNode = findAndRemoveCAstNode(castInst->getCallee().getOpaqueValue());
  // cast in CASt
  nodeMap->insert(std::make_pair(castInst, funcRefNode));
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
    char instrKindStr[80];
    *outs << "\t [SRC]: " << src.getOpaqueValue() << "\n";
    sprintf (instrKindStr, "instrKind: %d\n", src->getKind());
    *outs << instrKindStr;
    if (src->getKind() == ValueKind::SILPHIArgument) {
      *outs << "Yes, this is SILPHIArgument\n";
    }
    if (src->getKind() == ValueKind::SILFunctionArgument) {
      *outs << "Yes, this is SILFunctionArgument\n";
    }
    *outs << "\t [DEST]: " << dest.getOpaqueValue() << "\n";
    *outs << instrKindStr;
  }
  
  jobject new_node = nullptr;
  if (symbolTable->has(dest.getOpaqueValue())){
    jobject var = findAndRemoveCAstNode(dest.getOpaqueValue());
    new_node = (*wala)->makeNode(CAstWrapper::ASSIGN,var, findAndRemoveCAstNode(src.getOpaqueValue()));
  }

  // sometimes SIL creates temporary memory on the stack
  // the following code represents the correspondence between the origial value and the new temporary location
  if (nodeMap->find(src.getOpaqueValue()) != nodeMap->end()) {
    nodeMap->insert(std::make_pair(dest.getOpaqueValue(), nodeMap->at(src.getOpaqueValue())));
  }

  return new_node;
}

jobject InstrKindInfoGetter::handleBeginAccessInst() {
  if (outs != NULL) {
    *outs << "<< Begin Access >>" << "\n";
  }
  BeginAccessInst *castInst = cast<BeginAccessInst>(instr);
  if (outs != NULL) {
    *outs << "\t\t [oper_addr]:" << (castInst->getSource()).getOpaqueValue() << "\n";
  }
  jobject read_var = findAndRemoveCAstNode(castInst->getSource().getOpaqueValue());
  nodeMap->insert(std::make_pair(castInst, read_var));
  return nullptr;
}

jobject InstrKindInfoGetter::handleAssignInst() {
  *outs << "<<Assign Instruction>>" << "\n";
  AssignInst *castInst = cast<AssignInst>(instr);
  *outs << "[source]:" << castInst->getSrc().getOpaqueValue() << "\n";
  *outs << "[Dest]:" << castInst->getDest().getOpaqueValue() << "\n";
  jobject dest = findAndRemoveCAstNode(castInst->getDest().getOpaqueValue());
  jobject expr = findAndRemoveCAstNode(castInst->getSrc().getOpaqueValue());

  jobject assign_node = (*wala)->makeNode(CAstWrapper::ASSIGN, dest, expr);
  nodeMap->insert(std::make_pair(castInst,assign_node));
  return assign_node;
}

jobject InstrKindInfoGetter::handleAllocStackInst() {
  if (outs != NULL) {
    *outs << "<< AllocStack Instruction >>" << "\n";
  }
  AllocStackInst *castInst = cast<AllocStackInst>(instr);
  if (outs != NULL) {
    for (auto& operand : castInst->getAllOperands()) {
      *outs << "\t [OPERAND]: " << operand.get() << "\n";
      *outs << "\t [ADDR]: " << operand.get().getOpaqueValue() << "\n";
    }
  }
  return nullptr;
}

jobject InstrKindInfoGetter::handleReturnInst() {
  if (outs != NULL) {
    *outs << "<< ReturnInst >>" << "\n";
  }
  ReturnInst *castInst = cast<ReturnInst>(instr);
  SILValue return_val = castInst->getOperand();

  if (outs != NULL) {
    *outs << "operand:" << return_val << "\n";
    *outs << "addr:" << return_val.getOpaqueValue() << "\n";
  }
  jobject node = nullptr;
  if (return_val != NULL) {
    jobject val = nullptr;
    val = findAndRemoveCAstNode(return_val.getOpaqueValue());
    if (val == nullptr) {
      node = (*wala)->makeNode(CAstWrapper::RETURN);
    } else {
      node = (*wala)->makeNode(CAstWrapper::RETURN, val);
    }
    nodeMap->insert(std::make_pair(castInst, node));
  }
  return node;
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
      /*
      for(auto& argument : destBasicBlock->getArguments()){
        *outs << "addr of argument:" << argument << "\n";
      }*/
    }
  }
  if (destBasicBlock != NULL) {
    jobject labelNode = (*wala)->makeConstant(labeller->label(destBasicBlock).c_str());
    gotoNode = (*wala)->makeNode(CAstWrapper::GOTO, labelNode);
  }

  for(unsigned i = 0; i < castInst->getNumArgs(); i++){
    //*outs << "Argument:" << castInst->getArg(i) << "\n";
    *outs << "addr:" << destBasicBlock->getArgument(i) << "\n";
    jobject node = findAndRemoveCAstNode(castInst->getArg(i).getOpaqueValue());
    symbolTable->insert(destBasicBlock->getArgument(i), ("argument" + std::to_string(i)));

    jobject varName = (*wala)->makeConstant(symbolTable->get(destBasicBlock->getArgument(i)).c_str());
    jobject var = (*wala)->makeNode(CAstWrapper::VAR, varName);
    jobject assign = (*wala)->makeNode(CAstWrapper::ASSIGN, var, node);
    nodeList->push_back(assign);
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
  jobject condNode = findAndRemoveCAstNode(cond.getOpaqueValue());
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

jobject InstrKindInfoGetter::handleUnreachableInst() {
  if (outs != NULL) {
    *outs << "<< UnreachableInst >>" << "\n";
  }
  UnreachableInst* castInst = cast<UnreachableInst>(instr);
  if (outs != NULL) {
    if (castInst->isBranch()) {
      *outs << "This is a terminator of branch!" << "\n";
    }
    if (castInst->isFunctionExiting()) {
      *outs << "This is a terminator of function!" << "\n";
    }
  }
  return nullptr;
}

jobject InstrKindInfoGetter::handleCopyValueInst() {
  *outs << "<< CopyValueInst >>" << "\n";
  CopyValueInst *castInst = cast<CopyValueInst>(instr);
  *outs << "\t\t [name]:" << castInst->getOperand() << "\n";

  jobject node = findAndRemoveCAstNode(castInst->getOperand().getOpaqueValue());

  nodeMap->insert(std::make_pair(castInst, node));
  return node;
}

jobject InstrKindInfoGetter::handleAllocGlobalInst() {
  if (outs != NULL) {
    *outs << "<< AllocGlobalInst >>" << "\n";
  }
  AllocGlobalInst *castInst = cast<AllocGlobalInst>(instr);
  SILGlobalVariable* variable = castInst->getReferencedGlobal();
  StringRef var_name = variable->getName();
  SILType var_type = variable->getLoweredType();
  if (outs != NULL) {
    *outs << "\t\t[Var name]:" << var_name << "\n";
    *outs << "\t\t[Var type]:" << var_type << "\n";
  }
  return nullptr;
}

jobject InstrKindInfoGetter::handleGlobalAddrInst() {
  if (outs != NULL) {
    *outs << "<< GlobalAddrInst >>" << "\n";
  }
  GlobalAddrInst *castInst = cast<GlobalAddrInst>(instr);
  SILGlobalVariable* variable = castInst->getReferencedGlobal();
  StringRef var_name = variable->getName();
  if (outs != NULL) {
    *outs << "\t\t[Var name]:" << var_name << "\n";
  }
  //*outs << ((string)var_name).c_str() << "\n";
  //*outs << "\t\t[Addr]:" << init_inst << "\n";
  //jobject symbol = (*wala)->makeConstant(var_name.data());
  symbolTable->insert(castInst, var_name);
  return nullptr;
}

jobject InstrKindInfoGetter::handleTryApplyInst() {
  if (outs != NULL) {
    *outs << "<< TryApplyInst >>" << "\n";
  }
  TryApplyInst *castInst = cast<TryApplyInst>(instr);
  jobject funcExprNode = findAndRemoveCAstNode(castInst->getReferencedFunction());
  list<jobject> params;
  for (unsigned i = 0; i < castInst->getNumArguments(); ++i) {

    SILValue v = castInst->getArgument(i);
    jobject child = findAndRemoveCAstNode(v.getOpaqueValue());
    if (child != nullptr) {
      params.push_back(child);
    }

    if (outs != NULL) {
      *outs << "\t [ARG] #" << i << ": " << v;
      *outs << "\t [ADDR] #" << i << ": " << v.getOpaqueValue() << "\n";
    }
  }
  jobject call = (*wala)->makeNode(CAstWrapper::CALL, funcExprNode, (*wala)->makeArray(&params));
  jobject tryfunc = (*wala)->makeNode(CAstWrapper::TRY,call);
  jobject var_name = (*wala)->makeConstant("resulf_of_try");
  jobject var = (*wala)->makeNode(CAstWrapper::VAR,var_name);
  jobject node = (*wala)->makeNode(CAstWrapper::ASSIGN,var,tryfunc);
  nodeMap->insert(std::make_pair(castInst,node));
  SILBasicBlock *normalbb = castInst->getNormalBB();
  symbolTable->insert(normalbb->getArgument(0), "resulf_of_try"); // insert the node into the hash map
  return node;
}

ValueKind InstrKindInfoGetter::get() {
  ValueKind instrKind = instr->getKind();
  jobject node = nullptr;

  switch (instrKind) {
    case ValueKind::SILPHIArgument:
    case ValueKind::SILFunctionArgument:
    case ValueKind::SILUndef: {   
      *outs << "<< Not an instruction >>" << "\n";
      break;
    }
    
    case ValueKind::AllocBoxInst: {
      node  = handleAllocBoxInst();
      break;
    }
  
    case ValueKind::ApplyInst: {
      node = handleApplyInst();
      break;
    }
    
    case ValueKind::PartialApplyInst: {
      *outs << "<< PartialApplyInst >>" << "\n";
      break;
    }
    
    case ValueKind::IntegerLiteralInst: {
      node = handleIntegerLiteralInst();
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
      node = handleDebugValueInst();
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
      node = handleDebugValueInst();
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
      node = handleLoadInst();
      break;
    }
    
    case ValueKind::LoadBorrowInst: {
      node = handleLoadBorrowInst();
      break;
    }
    
    case ValueKind::BeginBorrowInst: {
      node = handleBeginBorrowInst();
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
    case ValueKind::ThickToObjCMetatypeInst:
    case ValueKind::ObjCToThickMetatypeInst:
    case ValueKind::ConvertFunctionInst:
    case ValueKind::ObjCExistentialMetatypeToObjectInst:
    case ValueKind::ObjCMetatypeToObjectInst: {
      *outs << "<< Conversion Instruction >>" << "\n";
      break;
    }
      
    case ValueKind::ThinToThickFunctionInst: {
      node = handleThinToThickFunctionInst();
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
      node = handleBeginAccessInst();
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
    
    case ValueKind::StoreBorrowInst:{
      *outs << "<< Store Borrow Instruction >>" << "\n";
      break;      
    }
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
      node = handleAllocStackInst();
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
      TupleInst *castInst = cast<TupleInst>(instr);
      //OperandValueArrayRef getElements()
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
      node = handleReturnInst();
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
      node = handleUnreachableInst();
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

    case ValueKind::CopyValueInst: {
      node = handleCopyValueInst();
      break;
    }

    case ValueKind::DestroyValueInst:{
      *outs << "<< DestroyValueInst >>" << "\n";
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
      node = handleAllocGlobalInst(); 
      break;
    }
    
    case ValueKind::GlobalAddrInst: { 
      node = handleGlobalAddrInst();
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
    
    case ValueKind::TryApplyInst: {
      node = handleTryApplyInst();
      break;
    }

    default: {
      *outs << "\t\t xxxxx Not a handled inst type \n";
      break;
    }
  }
  if (node != nullptr) {
    nodeList->push_back(node);
    //wala->print(node);
  }

  *outs << *instr << "\n";
  return instrKind;
}