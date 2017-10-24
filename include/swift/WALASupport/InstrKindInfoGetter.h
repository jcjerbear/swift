#ifndef SWIFT_INSTRKINDINFOGETTER_H
#define SWIFT_INSTRKINDINFOGETTER_H

#include "swift/WALASupport/WALAWalker.h" // included for WALAIntegration

namespace swift {

class InstrKindInfoGetter {
public:
	// If not NULL, debugging info will be printed via outs
	InstrKindInfoGetter(SILInstruction* instr, WALAIntegration* wala, unordered_map<void*, jobject>* allNodes, raw_ostream* outs = NULL);
	
	ValueKind get();
private:
	// member variables
	SILInstruction* instr;
	WALAIntegration* wala;
	unordered_map<void*, jobject>* allNodes;
	raw_ostream* outs;

	// member functions
	bool isBuiltInFunction(SILFunction* function);

	jobject handleApplyInst();
	jobject handleStringLiteralInst();
	jobject handleConstStringLiteralInst();
	jobject handleFunctionRefInst();
	jobject handleIntegerLiteralIns();
};

} // end namespace swift


#endif