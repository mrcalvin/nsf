
static int convertToInfomethodsubcmd(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"args", "body", "definition", "handle", "parameter", "parametersyntax", "type", "precondition", "postcondition", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "infomethodsubcmd", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum InfomethodsubcmdIdx {InfomethodsubcmdNULL, InfomethodsubcmdArgsIdx, InfomethodsubcmdBodyIdx, InfomethodsubcmdDefinitionIdx, InfomethodsubcmdHandleIdx, InfomethodsubcmdParameterIdx, InfomethodsubcmdParametersyntaxIdx, InfomethodsubcmdTypeIdx, InfomethodsubcmdPreconditionIdx, InfomethodsubcmdPostconditionIdx};
  
static int convertToMethodtype(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"all", "scripted", "builtin", "alias", "forwarder", "object", "setter", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "-methodtype", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum MethodtypeIdx {MethodtypeNULL, MethodtypeAllIdx, MethodtypeScriptedIdx, MethodtypeBuiltinIdx, MethodtypeAliasIdx, MethodtypeForwarderIdx, MethodtypeObjectIdx, MethodtypeSetterIdx};
  
static int convertToCallprotection(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"all", "protected", "public", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "-callprotection", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum CallprotectionIdx {CallprotectionNULL, CallprotectionAllIdx, CallprotectionProtectedIdx, CallprotectionPublicIdx};
  
static int convertToScope(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"all", "class", "object", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "-scope", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum ScopeIdx {ScopeNULL, ScopeAllIdx, ScopeClassIdx, ScopeObjectIdx};
  
static int convertToInfocallablesubcmd(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"filter", "method", "methods", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "infocallablesubcmd", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum InfocallablesubcmdIdx {InfocallablesubcmdNULL, InfocallablesubcmdFilterIdx, InfocallablesubcmdMethodIdx, InfocallablesubcmdMethodsIdx};
  
static int convertToAssertionsubcmd(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"check", "object-invar", "class-invar", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "assertionsubcmd", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum AssertionsubcmdIdx {AssertionsubcmdNULL, AssertionsubcmdCheckIdx, AssertionsubcmdObject_invarIdx, AssertionsubcmdClass_invarIdx};
  
static int convertToConfigureoption(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"filter", "softrecreate", "objectsystems", "keepinitcmd", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "configureoption", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum ConfigureoptionIdx {ConfigureoptionNULL, ConfigureoptionFilterIdx, ConfigureoptionSoftrecreateIdx, ConfigureoptionObjectsystemsIdx, ConfigureoptionKeepinitcmdIdx};
  
static int convertToCurrentoption(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"proc", "method", "object", "class", "activelevel", "args", "activemixin", "calledproc", "calledmethod", "calledclass", "callingproc", "callingmethod", "callingclass", "callinglevel", "callingobject", "filterreg", "isnextcall", "next", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "currentoption", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum CurrentoptionIdx {CurrentoptionNULL, CurrentoptionProcIdx, CurrentoptionMethodIdx, CurrentoptionObjectIdx, CurrentoptionClassIdx, CurrentoptionActivelevelIdx, CurrentoptionArgsIdx, CurrentoptionActivemixinIdx, CurrentoptionCalledprocIdx, CurrentoptionCalledmethodIdx, CurrentoptionCalledclassIdx, CurrentoptionCallingprocIdx, CurrentoptionCallingmethodIdx, CurrentoptionCallingclassIdx, CurrentoptionCallinglevelIdx, CurrentoptionCallingobjectIdx, CurrentoptionFilterregIdx, CurrentoptionIsnextcallIdx, CurrentoptionNextIdx};
  
static int convertToMethodproperty(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"class-only", "protected", "redefine-protected", "returns", "slotobj", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "methodproperty", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum MethodpropertyIdx {MethodpropertyNULL, MethodpropertyClass_onlyIdx, MethodpropertyProtectedIdx, MethodpropertyRedefine_protectedIdx, MethodpropertyReturnsIdx, MethodpropertySlotobjIdx};
  
static int convertToObjectkind(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"type", "object", "class", "baseclass", "metaclass", "hasmixin", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "objectkind", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum ObjectkindIdx {ObjectkindNULL, ObjectkindTypeIdx, ObjectkindObjectIdx, ObjectkindClassIdx, ObjectkindBaseclassIdx, ObjectkindMetaclassIdx, ObjectkindHasmixinIdx};
  
static int convertToRelationtype(Tcl_Interp *interp, Tcl_Obj *objPtr, XOTclParam CONST *pPtr, 
			    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int index, result;
  static CONST char *opts[] = {"object-mixin", "class-mixin", "object-filter", "class-filter", "class", "superclass", "rootclass", NULL};
  result = Tcl_GetIndexFromObj(interp, objPtr, opts, "relationtype", 0, &index);
  *clientData = (ClientData) INT2PTR(index + 1);
  *outObjPtr = objPtr;
  return result;
}
enum RelationtypeIdx {RelationtypeNULL, RelationtypeObject_mixinIdx, RelationtypeClass_mixinIdx, RelationtypeObject_filterIdx, RelationtypeClass_filterIdx, RelationtypeClassIdx, RelationtypeSuperclassIdx, RelationtypeRootclassIdx};
  

typedef struct {
  CONST char *methodName;
  Tcl_ObjCmdProc *proc;
  int nrParameters;
  XOTclParam paramDefs[12];
} methodDefinition;

static int ArgumentParse(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], 
                         XOTclObject *obj, Tcl_Obj *procName,
                         XOTclParam CONST *paramPtr, int nrParameters, parseContext *pc);

static int getMatchObject(Tcl_Interp *interp, Tcl_Obj *patternObj, Tcl_Obj *origObj,
			  XOTclObject **matchObject, CONST char **pattern);

/* just to define the symbol */
static methodDefinition method_definitions[];
  
static CONST char *method_command_namespace_names[] = {
  "::nsf::cmd::ObjectInfo",
  "::nsf::cmd::Object",
  "::nsf::cmd::ClassInfo",
  "::nsf::cmd::ParameterType",
  "::nsf::cmd::Class"
};
static int XOTclCAllocMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCCreateMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCDeallocMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCFilterGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCMixinGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCNewMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCRecreateMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoFilterMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoForwardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoHeritageMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoInstancesMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoMethodMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoMethodsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoMixinMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoMixinOfMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoSlotsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoSubclassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclClassInfoSuperclassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoCallableMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoChildrenMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoClassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoFilterMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoForwardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoHasnamespaceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoMethodMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoMethodsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoMixinMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoParentMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoPrecedenceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoSlotObjectsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjInfoVarsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOAutonameMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOCleanupMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOConfigureMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclODestroyMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOExistsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOFilterGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOInstVarMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOMixinGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclONoinitMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclORequireNamespaceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOResidualargsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOUplevelMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOUpvarMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOVarsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOVolatileMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclOVwaitMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclAliasCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclAssertionCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclColonCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclConfigureCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCreateObjectSystemCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclCurrentCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclDeprecatedCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclDispatchCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclExistsVarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclFinalizeObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclForwardCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclImportvarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclInterpObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclInvalidateObjectParameterCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclIsCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclMethodCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclMethodPropertyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclMyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclNSCopyCmdsStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclNSCopyVarsStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclObjectpropertyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclParametercheckCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclQualifyObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclRelationCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclSetVarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);
static int XOTclSetterCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv []);

static int XOTclCAllocMethod(Tcl_Interp *interp, XOTclClass *cl, Tcl_Obj *name);
static int XOTclCCreateMethod(Tcl_Interp *interp, XOTclClass *cl, CONST char *name, int objc, Tcl_Obj *CONST objv[]);
static int XOTclCDeallocMethod(Tcl_Interp *interp, XOTclClass *cl, Tcl_Obj *object);
static int XOTclCFilterGuardMethod(Tcl_Interp *interp, XOTclClass *cl, CONST char *filter, Tcl_Obj *guard);
static int XOTclCMixinGuardMethod(Tcl_Interp *interp, XOTclClass *cl, CONST char *mixin, Tcl_Obj *guard);
static int XOTclCNewMethod(Tcl_Interp *interp, XOTclClass *cl, XOTclObject *withChildof, int nobjc, Tcl_Obj *CONST nobjv[]);
static int XOTclCRecreateMethod(Tcl_Interp *interp, XOTclClass *cl, Tcl_Obj *name, int objc, Tcl_Obj *CONST objv[]);
static int XOTclClassInfoFilterMethod(Tcl_Interp *interp, XOTclClass *class, int withGuard, int withGuards, CONST char *pattern);
static int XOTclClassInfoForwardMethod(Tcl_Interp *interp, XOTclClass *class, int withDefinition, CONST char *name);
static int XOTclClassInfoHeritageMethod(Tcl_Interp *interp, XOTclClass *class, CONST char *pattern);
static int XOTclClassInfoInstancesMethod(Tcl_Interp *interp, XOTclClass *class, int withClosure, CONST char *patternString, XOTclObject *patternObj);
static int XOTclClassInfoMethodMethod(Tcl_Interp *interp, XOTclClass *class, int infomethodsubcmd, CONST char *name);
static int XOTclClassInfoMethodsMethod(Tcl_Interp *interp, XOTclClass *class, int withMethodtype, int withCallprotection, int withNomixins, int withIncontext, CONST char *pattern);
static int XOTclClassInfoMixinMethod(Tcl_Interp *interp, XOTclClass *class, int withClosure, int withGuard, int withGuards, CONST char *patternString, XOTclObject *patternObj);
static int XOTclClassInfoMixinOfMethod(Tcl_Interp *interp, XOTclClass *class, int withClosure, int withScope, CONST char *patternString, XOTclObject *patternObj);
static int XOTclClassInfoSlotsMethod(Tcl_Interp *interp, XOTclClass *class);
static int XOTclClassInfoSubclassMethod(Tcl_Interp *interp, XOTclClass *class, int withClosure, CONST char *patternString, XOTclObject *patternObj);
static int XOTclClassInfoSuperclassMethod(Tcl_Interp *interp, XOTclClass *class, int withClosure, Tcl_Obj *pattern);
static int XOTclObjInfoCallableMethod(Tcl_Interp *interp, XOTclObject *object, int infocallablesubcmd, int withMethodtype, int withCallprotection, int withApplication, int withNomixins, int withIncontext, CONST char *pattern);
static int XOTclObjInfoChildrenMethod(Tcl_Interp *interp, XOTclObject *object, CONST char *pattern);
static int XOTclObjInfoClassMethod(Tcl_Interp *interp, XOTclObject *object);
static int XOTclObjInfoFilterMethod(Tcl_Interp *interp, XOTclObject *object, int withGuard, int withGuards, int withOrder, CONST char *pattern);
static int XOTclObjInfoForwardMethod(Tcl_Interp *interp, XOTclObject *object, int withDefinition, CONST char *name);
static int XOTclObjInfoHasnamespaceMethod(Tcl_Interp *interp, XOTclObject *object);
static int XOTclObjInfoMethodMethod(Tcl_Interp *interp, XOTclObject *object, int infomethodsubcmd, CONST char *name);
static int XOTclObjInfoMethodsMethod(Tcl_Interp *interp, XOTclObject *object, int withMethodtype, int withCallprotection, int withNomixins, int withIncontext, CONST char *pattern);
static int XOTclObjInfoMixinMethod(Tcl_Interp *interp, XOTclObject *object, int withGuard, int withGuards, int withOrder, CONST char *patternString, XOTclObject *patternObj);
static int XOTclObjInfoParentMethod(Tcl_Interp *interp, XOTclObject *object);
static int XOTclObjInfoPrecedenceMethod(Tcl_Interp *interp, XOTclObject *object, int withIntrinsic, CONST char *pattern);
static int XOTclObjInfoSlotObjectsMethod(Tcl_Interp *interp, XOTclObject *object, CONST char *pattern);
static int XOTclObjInfoVarsMethod(Tcl_Interp *interp, XOTclObject *object, CONST char *pattern);
static int XOTclOAutonameMethod(Tcl_Interp *interp, XOTclObject *obj, int withInstance, int withReset, Tcl_Obj *name);
static int XOTclOCleanupMethod(Tcl_Interp *interp, XOTclObject *obj);
static int XOTclOConfigureMethod(Tcl_Interp *interp, XOTclObject *obj, int objc, Tcl_Obj *CONST objv[]);
static int XOTclODestroyMethod(Tcl_Interp *interp, XOTclObject *obj);
static int XOTclOExistsMethod(Tcl_Interp *interp, XOTclObject *obj, CONST char *var);
static int XOTclOFilterGuardMethod(Tcl_Interp *interp, XOTclObject *obj, CONST char *filter, Tcl_Obj *guard);
static int XOTclOInstVarMethod(Tcl_Interp *interp, XOTclObject *obj, int objc, Tcl_Obj *CONST objv[]);
static int XOTclOMixinGuardMethod(Tcl_Interp *interp, XOTclObject *obj, CONST char *mixin, Tcl_Obj *guard);
static int XOTclONoinitMethod(Tcl_Interp *interp, XOTclObject *obj);
static int XOTclORequireNamespaceMethod(Tcl_Interp *interp, XOTclObject *obj);
static int XOTclOResidualargsMethod(Tcl_Interp *interp, XOTclObject *obj, int objc, Tcl_Obj *CONST objv[]);
static int XOTclOUplevelMethod(Tcl_Interp *interp, XOTclObject *obj, int objc, Tcl_Obj *CONST objv[]);
static int XOTclOUpvarMethod(Tcl_Interp *interp, XOTclObject *obj, int objc, Tcl_Obj *CONST objv[]);
static int XOTclOVarsMethod(Tcl_Interp *interp, XOTclObject *obj, CONST char *pattern);
static int XOTclOVolatileMethod(Tcl_Interp *interp, XOTclObject *obj);
static int XOTclOVwaitMethod(Tcl_Interp *interp, XOTclObject *obj, CONST char *varname);
static int XOTclAliasCmd(Tcl_Interp *interp, XOTclObject *object, int withPer_object, CONST char *methodName, int withNonleaf, int withObjscope, Tcl_Obj *cmdName);
static int XOTclAssertionCmd(Tcl_Interp *interp, XOTclObject *object, int assertionsubcmd, Tcl_Obj *arg);
static int XOTclColonCmd(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int XOTclConfigureCmd(Tcl_Interp *interp, int configureoption, Tcl_Obj *value);
static int XOTclCreateObjectSystemCmd(Tcl_Interp *interp, Tcl_Obj *rootClass, Tcl_Obj *rootMetaClass, Tcl_Obj *systemMethods);
static int XOTclCurrentCmd(Tcl_Interp *interp, int currentoption);
static int XOTclDeprecatedCmd(Tcl_Interp *interp, CONST char *what, CONST char *oldCmd, CONST char *newCmd);
static int XOTclDispatchCmd(Tcl_Interp *interp, XOTclObject *object, int withObjscope, Tcl_Obj *command, int nobjc, Tcl_Obj *CONST nobjv[]);
static int XOTclExistsVarCmd(Tcl_Interp *interp, XOTclObject *object, CONST char *var);
static int XOTclFinalizeObjCmd(Tcl_Interp *interp);
static int XOTclForwardCmd(Tcl_Interp *interp, XOTclObject *object, int withPer_object, Tcl_Obj *method, Tcl_Obj *withDefault, int withEarlybinding, Tcl_Obj *withMethodprefix, int withObjscope, Tcl_Obj *withOnerror, int withVerbose, Tcl_Obj *target, int nobjc, Tcl_Obj *CONST nobjv[]);
static int XOTclImportvarCmd(Tcl_Interp *interp, XOTclObject *object, int nobjc, Tcl_Obj *CONST nobjv[]);
static int XOTclInterpObjCmd(Tcl_Interp *interp, CONST char *name, int objc, Tcl_Obj *CONST objv[]);
static int XOTclInvalidateObjectParameterCmd(Tcl_Interp *interp, XOTclClass *class);
static int XOTclIsCmd(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Obj *constraint, Tcl_Obj *withHasmixin, Tcl_Obj *withType, Tcl_Obj *arg);
static int XOTclMethodCmd(Tcl_Interp *interp, XOTclObject *object, int withInner_namespace, int withPer_object, int withPublic, Tcl_Obj *name, Tcl_Obj *args, Tcl_Obj *body, Tcl_Obj *withPrecondition, Tcl_Obj *withPostcondition);
static int XOTclMethodPropertyCmd(Tcl_Interp *interp, XOTclObject *object, int withPer_object, Tcl_Obj *methodName, int methodproperty, Tcl_Obj *value);
static int XOTclMyCmd(Tcl_Interp *interp, int withLocal, Tcl_Obj *method, int nobjc, Tcl_Obj *CONST nobjv[]);
static int XOTclNSCopyCmds(Tcl_Interp *interp, Tcl_Obj *fromNs, Tcl_Obj *toNs);
static int XOTclNSCopyVars(Tcl_Interp *interp, Tcl_Obj *fromNs, Tcl_Obj *toNs);
static int XOTclObjectpropertyCmd(Tcl_Interp *interp, Tcl_Obj *object, int objectkind, Tcl_Obj *value);
static int XOTclParametercheckCmd(Tcl_Interp *interp, int withNocomplain, Tcl_Obj *param, Tcl_Obj *value);
static int XOTclQualifyObjCmd(Tcl_Interp *interp, Tcl_Obj *name);
static int XOTclRelationCmd(Tcl_Interp *interp, XOTclObject *object, int relationtype, Tcl_Obj *value);
static int XOTclSetVarCmd(Tcl_Interp *interp, XOTclObject *object, Tcl_Obj *variable, Tcl_Obj *value);
static int XOTclSetterCmd(Tcl_Interp *interp, XOTclObject *object, int withPer_object, Tcl_Obj *parameter);

enum {
 XOTclCAllocMethodIdx,
 XOTclCCreateMethodIdx,
 XOTclCDeallocMethodIdx,
 XOTclCFilterGuardMethodIdx,
 XOTclCMixinGuardMethodIdx,
 XOTclCNewMethodIdx,
 XOTclCRecreateMethodIdx,
 XOTclClassInfoFilterMethodIdx,
 XOTclClassInfoForwardMethodIdx,
 XOTclClassInfoHeritageMethodIdx,
 XOTclClassInfoInstancesMethodIdx,
 XOTclClassInfoMethodMethodIdx,
 XOTclClassInfoMethodsMethodIdx,
 XOTclClassInfoMixinMethodIdx,
 XOTclClassInfoMixinOfMethodIdx,
 XOTclClassInfoSlotsMethodIdx,
 XOTclClassInfoSubclassMethodIdx,
 XOTclClassInfoSuperclassMethodIdx,
 XOTclObjInfoCallableMethodIdx,
 XOTclObjInfoChildrenMethodIdx,
 XOTclObjInfoClassMethodIdx,
 XOTclObjInfoFilterMethodIdx,
 XOTclObjInfoForwardMethodIdx,
 XOTclObjInfoHasnamespaceMethodIdx,
 XOTclObjInfoMethodMethodIdx,
 XOTclObjInfoMethodsMethodIdx,
 XOTclObjInfoMixinMethodIdx,
 XOTclObjInfoParentMethodIdx,
 XOTclObjInfoPrecedenceMethodIdx,
 XOTclObjInfoSlotObjectsMethodIdx,
 XOTclObjInfoVarsMethodIdx,
 XOTclOAutonameMethodIdx,
 XOTclOCleanupMethodIdx,
 XOTclOConfigureMethodIdx,
 XOTclODestroyMethodIdx,
 XOTclOExistsMethodIdx,
 XOTclOFilterGuardMethodIdx,
 XOTclOInstVarMethodIdx,
 XOTclOMixinGuardMethodIdx,
 XOTclONoinitMethodIdx,
 XOTclORequireNamespaceMethodIdx,
 XOTclOResidualargsMethodIdx,
 XOTclOUplevelMethodIdx,
 XOTclOUpvarMethodIdx,
 XOTclOVarsMethodIdx,
 XOTclOVolatileMethodIdx,
 XOTclOVwaitMethodIdx,
 XOTclAliasCmdIdx,
 XOTclAssertionCmdIdx,
 XOTclColonCmdIdx,
 XOTclConfigureCmdIdx,
 XOTclCreateObjectSystemCmdIdx,
 XOTclCurrentCmdIdx,
 XOTclDeprecatedCmdIdx,
 XOTclDispatchCmdIdx,
 XOTclExistsVarCmdIdx,
 XOTclFinalizeObjCmdIdx,
 XOTclForwardCmdIdx,
 XOTclImportvarCmdIdx,
 XOTclInterpObjCmdIdx,
 XOTclInvalidateObjectParameterCmdIdx,
 XOTclIsCmdIdx,
 XOTclMethodCmdIdx,
 XOTclMethodPropertyCmdIdx,
 XOTclMyCmdIdx,
 XOTclNSCopyCmdsIdx,
 XOTclNSCopyVarsIdx,
 XOTclObjectpropertyCmdIdx,
 XOTclParametercheckCmdIdx,
 XOTclQualifyObjCmdIdx,
 XOTclRelationCmdIdx,
 XOTclSetVarCmdIdx,
 XOTclSetterCmdIdx
} XOTclMethods;


static int
XOTclCAllocMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCAllocMethodIdx].paramDefs, 
                     method_definitions[XOTclCAllocMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *name = (Tcl_Obj *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclCAllocMethod(interp, cl, name);

  }
}

static int
XOTclCCreateMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCCreateMethodIdx].paramDefs, 
                     method_definitions[XOTclCCreateMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *name = (CONST char *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclCCreateMethod(interp, cl, name, objc, objv);

  }
}

static int
XOTclCDeallocMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCDeallocMethodIdx].paramDefs, 
                     method_definitions[XOTclCDeallocMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *object = (Tcl_Obj *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclCDeallocMethod(interp, cl, object);

  }
}

static int
XOTclCFilterGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCFilterGuardMethodIdx].paramDefs, 
                     method_definitions[XOTclCFilterGuardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *filter = (CONST char *)pc.clientData[0];
    Tcl_Obj *guard = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclCFilterGuardMethod(interp, cl, filter, guard);

  }
}

static int
XOTclCMixinGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCMixinGuardMethodIdx].paramDefs, 
                     method_definitions[XOTclCMixinGuardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *mixin = (CONST char *)pc.clientData[0];
    Tcl_Obj *guard = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclCMixinGuardMethod(interp, cl, mixin, guard);

  }
}

static int
XOTclCNewMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCNewMethodIdx].paramDefs, 
                     method_definitions[XOTclCNewMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *withChildof = (XOTclObject *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclCNewMethod(interp, cl, withChildof, objc-pc.lastobjc, objv+pc.lastobjc);

  }
}

static int
XOTclCRecreateMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclClass *cl =  XOTclObjectToClass(clientData);
  if (!cl) return XOTclObjErrType(interp, objv[0], "Class", "");
  if (ArgumentParse(interp, objc, objv, (XOTclObject *) cl, objv[0], 
                     method_definitions[XOTclCRecreateMethodIdx].paramDefs, 
                     method_definitions[XOTclCRecreateMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *name = (Tcl_Obj *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclCRecreateMethod(interp, cl, name, objc, objv);

  }
}

static int
XOTclClassInfoFilterMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoFilterMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoFilterMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withGuard = (int )PTR2INT(pc.clientData[1]);
    int withGuards = (int )PTR2INT(pc.clientData[2]);
    CONST char *pattern = (CONST char *)pc.clientData[3];

    parseContextRelease(&pc);
    return XOTclClassInfoFilterMethod(interp, class, withGuard, withGuards, pattern);

  }
}

static int
XOTclClassInfoForwardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoForwardMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoForwardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withDefinition = (int )PTR2INT(pc.clientData[1]);
    CONST char *name = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclClassInfoForwardMethod(interp, class, withDefinition, name);

  }
}

static int
XOTclClassInfoHeritageMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoHeritageMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoHeritageMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    CONST char *pattern = (CONST char *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclClassInfoHeritageMethod(interp, class, pattern);

  }
}

static int
XOTclClassInfoInstancesMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoInstancesMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoInstancesMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withClosure = (int )PTR2INT(pc.clientData[1]);
    CONST char *patternString = NULL;
    XOTclObject *patternObj = NULL;
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[2];
    int returnCode;

    if (getMatchObject(interp, pattern, objc>2 ? objv[2] : NULL, &patternObj, &patternString) == -1) {
      if (pattern) {
        DECR_REF_COUNT(pattern);
      }
      return TCL_OK;
    }
          
    parseContextRelease(&pc);
    returnCode = XOTclClassInfoInstancesMethod(interp, class, withClosure, patternString, patternObj);

    if (pattern) {
      DECR_REF_COUNT(pattern);
    }
    return returnCode;
  }
}

static int
XOTclClassInfoMethodMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoMethodMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoMethodMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int infomethodsubcmd = (int )PTR2INT(pc.clientData[1]);
    CONST char *name = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclClassInfoMethodMethod(interp, class, infomethodsubcmd, name);

  }
}

static int
XOTclClassInfoMethodsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoMethodsMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoMethodsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withMethodtype = (int )PTR2INT(pc.clientData[1]);
    int withCallprotection = (int )PTR2INT(pc.clientData[2]);
    int withNomixins = (int )PTR2INT(pc.clientData[3]);
    int withIncontext = (int )PTR2INT(pc.clientData[4]);
    CONST char *pattern = (CONST char *)pc.clientData[5];

    parseContextRelease(&pc);
    return XOTclClassInfoMethodsMethod(interp, class, withMethodtype, withCallprotection, withNomixins, withIncontext, pattern);

  }
}

static int
XOTclClassInfoMixinMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoMixinMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoMixinMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withClosure = (int )PTR2INT(pc.clientData[1]);
    int withGuard = (int )PTR2INT(pc.clientData[2]);
    int withGuards = (int )PTR2INT(pc.clientData[3]);
    CONST char *patternString = NULL;
    XOTclObject *patternObj = NULL;
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[4];
    int returnCode;

    if (getMatchObject(interp, pattern, objc>4 ? objv[4] : NULL, &patternObj, &patternString) == -1) {
      if (pattern) {
        DECR_REF_COUNT(pattern);
      }
      return TCL_OK;
    }
          
    parseContextRelease(&pc);
    returnCode = XOTclClassInfoMixinMethod(interp, class, withClosure, withGuard, withGuards, patternString, patternObj);

    if (pattern) {
      DECR_REF_COUNT(pattern);
    }
    return returnCode;
  }
}

static int
XOTclClassInfoMixinOfMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoMixinOfMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoMixinOfMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withClosure = (int )PTR2INT(pc.clientData[1]);
    int withScope = (int )PTR2INT(pc.clientData[2]);
    CONST char *patternString = NULL;
    XOTclObject *patternObj = NULL;
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[3];
    int returnCode;

    if (getMatchObject(interp, pattern, objc>3 ? objv[3] : NULL, &patternObj, &patternString) == -1) {
      if (pattern) {
        DECR_REF_COUNT(pattern);
      }
      return TCL_OK;
    }
          
    parseContextRelease(&pc);
    returnCode = XOTclClassInfoMixinOfMethod(interp, class, withClosure, withScope, patternString, patternObj);

    if (pattern) {
      DECR_REF_COUNT(pattern);
    }
    return returnCode;
  }
}

static int
XOTclClassInfoSlotsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoSlotsMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoSlotsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclClassInfoSlotsMethod(interp, class);

  }
}

static int
XOTclClassInfoSubclassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoSubclassMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoSubclassMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withClosure = (int )PTR2INT(pc.clientData[1]);
    CONST char *patternString = NULL;
    XOTclObject *patternObj = NULL;
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[2];
    int returnCode;

    if (getMatchObject(interp, pattern, objc>2 ? objv[2] : NULL, &patternObj, &patternString) == -1) {
      if (pattern) {
        DECR_REF_COUNT(pattern);
      }
      return TCL_OK;
    }
          
    parseContextRelease(&pc);
    returnCode = XOTclClassInfoSubclassMethod(interp, class, withClosure, patternString, patternObj);

    if (pattern) {
      DECR_REF_COUNT(pattern);
    }
    return returnCode;
  }
}

static int
XOTclClassInfoSuperclassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclClassInfoSuperclassMethodIdx].paramDefs, 
                     method_definitions[XOTclClassInfoSuperclassMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];
    int withClosure = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclClassInfoSuperclassMethod(interp, class, withClosure, pattern);

  }
}

static int
XOTclObjInfoCallableMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoCallableMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoCallableMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int infocallablesubcmd = (int )PTR2INT(pc.clientData[1]);
    int withMethodtype = (int )PTR2INT(pc.clientData[2]);
    int withCallprotection = (int )PTR2INT(pc.clientData[3]);
    int withApplication = (int )PTR2INT(pc.clientData[4]);
    int withNomixins = (int )PTR2INT(pc.clientData[5]);
    int withIncontext = (int )PTR2INT(pc.clientData[6]);
    CONST char *pattern = (CONST char *)pc.clientData[7];

    parseContextRelease(&pc);
    return XOTclObjInfoCallableMethod(interp, object, infocallablesubcmd, withMethodtype, withCallprotection, withApplication, withNomixins, withIncontext, pattern);

  }
}

static int
XOTclObjInfoChildrenMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoChildrenMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoChildrenMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    CONST char *pattern = (CONST char *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclObjInfoChildrenMethod(interp, object, pattern);

  }
}

static int
XOTclObjInfoClassMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoClassMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoClassMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclObjInfoClassMethod(interp, object);

  }
}

static int
XOTclObjInfoFilterMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoFilterMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoFilterMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withGuard = (int )PTR2INT(pc.clientData[1]);
    int withGuards = (int )PTR2INT(pc.clientData[2]);
    int withOrder = (int )PTR2INT(pc.clientData[3]);
    CONST char *pattern = (CONST char *)pc.clientData[4];

    parseContextRelease(&pc);
    return XOTclObjInfoFilterMethod(interp, object, withGuard, withGuards, withOrder, pattern);

  }
}

static int
XOTclObjInfoForwardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoForwardMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoForwardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withDefinition = (int )PTR2INT(pc.clientData[1]);
    CONST char *name = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclObjInfoForwardMethod(interp, object, withDefinition, name);

  }
}

static int
XOTclObjInfoHasnamespaceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoHasnamespaceMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoHasnamespaceMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclObjInfoHasnamespaceMethod(interp, object);

  }
}

static int
XOTclObjInfoMethodMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoMethodMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoMethodMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int infomethodsubcmd = (int )PTR2INT(pc.clientData[1]);
    CONST char *name = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclObjInfoMethodMethod(interp, object, infomethodsubcmd, name);

  }
}

static int
XOTclObjInfoMethodsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoMethodsMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoMethodsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withMethodtype = (int )PTR2INT(pc.clientData[1]);
    int withCallprotection = (int )PTR2INT(pc.clientData[2]);
    int withNomixins = (int )PTR2INT(pc.clientData[3]);
    int withIncontext = (int )PTR2INT(pc.clientData[4]);
    CONST char *pattern = (CONST char *)pc.clientData[5];

    parseContextRelease(&pc);
    return XOTclObjInfoMethodsMethod(interp, object, withMethodtype, withCallprotection, withNomixins, withIncontext, pattern);

  }
}

static int
XOTclObjInfoMixinMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoMixinMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoMixinMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withGuard = (int )PTR2INT(pc.clientData[1]);
    int withGuards = (int )PTR2INT(pc.clientData[2]);
    int withOrder = (int )PTR2INT(pc.clientData[3]);
    CONST char *patternString = NULL;
    XOTclObject *patternObj = NULL;
    Tcl_Obj *pattern = (Tcl_Obj *)pc.clientData[4];
    int returnCode;

    if (getMatchObject(interp, pattern, objc>4 ? objv[4] : NULL, &patternObj, &patternString) == -1) {
      if (pattern) {
        DECR_REF_COUNT(pattern);
      }
      return TCL_OK;
    }
          
    parseContextRelease(&pc);
    returnCode = XOTclObjInfoMixinMethod(interp, object, withGuard, withGuards, withOrder, patternString, patternObj);

    if (pattern) {
      DECR_REF_COUNT(pattern);
    }
    return returnCode;
  }
}

static int
XOTclObjInfoParentMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoParentMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoParentMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclObjInfoParentMethod(interp, object);

  }
}

static int
XOTclObjInfoPrecedenceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoPrecedenceMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoPrecedenceMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withIntrinsic = (int )PTR2INT(pc.clientData[1]);
    CONST char *pattern = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclObjInfoPrecedenceMethod(interp, object, withIntrinsic, pattern);

  }
}

static int
XOTclObjInfoSlotObjectsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoSlotObjectsMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoSlotObjectsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    CONST char *pattern = (CONST char *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclObjInfoSlotObjectsMethod(interp, object, pattern);

  }
}

static int
XOTclObjInfoVarsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjInfoVarsMethodIdx].paramDefs, 
                     method_definitions[XOTclObjInfoVarsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    CONST char *pattern = (CONST char *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclObjInfoVarsMethod(interp, object, pattern);

  }
}

static int
XOTclOAutonameMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOAutonameMethodIdx].paramDefs, 
                     method_definitions[XOTclOAutonameMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    int withInstance = (int )PTR2INT(pc.clientData[0]);
    int withReset = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *name = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclOAutonameMethod(interp, obj, withInstance, withReset, name);

  }
}

static int
XOTclOCleanupMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOCleanupMethodIdx].paramDefs, 
                     method_definitions[XOTclOCleanupMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclOCleanupMethod(interp, obj);

  }
}

static int
XOTclOConfigureMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
    

    return XOTclOConfigureMethod(interp, obj, objc, objv);

}

static int
XOTclODestroyMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclODestroyMethodIdx].paramDefs, 
                     method_definitions[XOTclODestroyMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclODestroyMethod(interp, obj);

  }
}

static int
XOTclOExistsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOExistsMethodIdx].paramDefs, 
                     method_definitions[XOTclOExistsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *var = (CONST char *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclOExistsMethod(interp, obj, var);

  }
}

static int
XOTclOFilterGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOFilterGuardMethodIdx].paramDefs, 
                     method_definitions[XOTclOFilterGuardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *filter = (CONST char *)pc.clientData[0];
    Tcl_Obj *guard = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclOFilterGuardMethod(interp, obj, filter, guard);

  }
}

static int
XOTclOInstVarMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
    

    return XOTclOInstVarMethod(interp, obj, objc, objv);

}

static int
XOTclOMixinGuardMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOMixinGuardMethodIdx].paramDefs, 
                     method_definitions[XOTclOMixinGuardMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *mixin = (CONST char *)pc.clientData[0];
    Tcl_Obj *guard = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclOMixinGuardMethod(interp, obj, mixin, guard);

  }
}

static int
XOTclONoinitMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclONoinitMethodIdx].paramDefs, 
                     method_definitions[XOTclONoinitMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclONoinitMethod(interp, obj);

  }
}

static int
XOTclORequireNamespaceMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclORequireNamespaceMethodIdx].paramDefs, 
                     method_definitions[XOTclORequireNamespaceMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclORequireNamespaceMethod(interp, obj);

  }
}

static int
XOTclOResidualargsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
    

    return XOTclOResidualargsMethod(interp, obj, objc, objv);

}

static int
XOTclOUplevelMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
    

    return XOTclOUplevelMethod(interp, obj, objc, objv);

}

static int
XOTclOUpvarMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
    

    return XOTclOUpvarMethod(interp, obj, objc, objv);

}

static int
XOTclOVarsMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOVarsMethodIdx].paramDefs, 
                     method_definitions[XOTclOVarsMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *pattern = (CONST char *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclOVarsMethod(interp, obj, pattern);

  }
}

static int
XOTclOVolatileMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOVolatileMethodIdx].paramDefs, 
                     method_definitions[XOTclOVolatileMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclOVolatileMethod(interp, obj);

  }
}

static int
XOTclOVwaitMethodStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;
  XOTclObject *obj =  (XOTclObject *)clientData;
  if (!obj) return XOTclObjErrType(interp, objv[0], "Object", "");
  if (ArgumentParse(interp, objc, objv, obj, objv[0], 
                     method_definitions[XOTclOVwaitMethodIdx].paramDefs, 
                     method_definitions[XOTclOVwaitMethodIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *varname = (CONST char *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclOVwaitMethod(interp, obj, varname);

  }
}

static int
XOTclAliasCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclAliasCmdIdx].paramDefs, 
                     method_definitions[XOTclAliasCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withPer_object = (int )PTR2INT(pc.clientData[1]);
    CONST char *methodName = (CONST char *)pc.clientData[2];
    int withNonleaf = (int )PTR2INT(pc.clientData[3]);
    int withObjscope = (int )PTR2INT(pc.clientData[4]);
    Tcl_Obj *cmdName = (Tcl_Obj *)pc.clientData[5];

    parseContextRelease(&pc);
    return XOTclAliasCmd(interp, object, withPer_object, methodName, withNonleaf, withObjscope, cmdName);

  }
}

static int
XOTclAssertionCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclAssertionCmdIdx].paramDefs, 
                     method_definitions[XOTclAssertionCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int assertionsubcmd = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *arg = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclAssertionCmd(interp, object, assertionsubcmd, arg);

  }
}

static int
XOTclColonCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {

    

    return XOTclColonCmd(interp, objc, objv);

}

static int
XOTclConfigureCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclConfigureCmdIdx].paramDefs, 
                     method_definitions[XOTclConfigureCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    int configureoption = (int )PTR2INT(pc.clientData[0]);
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclConfigureCmd(interp, configureoption, value);

  }
}

static int
XOTclCreateObjectSystemCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclCreateObjectSystemCmdIdx].paramDefs, 
                     method_definitions[XOTclCreateObjectSystemCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *rootClass = (Tcl_Obj *)pc.clientData[0];
    Tcl_Obj *rootMetaClass = (Tcl_Obj *)pc.clientData[1];
    Tcl_Obj *systemMethods = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclCreateObjectSystemCmd(interp, rootClass, rootMetaClass, systemMethods);

  }
}

static int
XOTclCurrentCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclCurrentCmdIdx].paramDefs, 
                     method_definitions[XOTclCurrentCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    int currentoption = (int )PTR2INT(pc.clientData[0]);

    parseContextRelease(&pc);
    return XOTclCurrentCmd(interp, currentoption);

  }
}

static int
XOTclDeprecatedCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclDeprecatedCmdIdx].paramDefs, 
                     method_definitions[XOTclDeprecatedCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *what = (CONST char *)pc.clientData[0];
    CONST char *oldCmd = (CONST char *)pc.clientData[1];
    CONST char *newCmd = (CONST char *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclDeprecatedCmd(interp, what, oldCmd, newCmd);

  }
}

static int
XOTclDispatchCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclDispatchCmdIdx].paramDefs, 
                     method_definitions[XOTclDispatchCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withObjscope = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *command = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclDispatchCmd(interp, object, withObjscope, command, objc-pc.lastobjc, objv+pc.lastobjc);

  }
}

static int
XOTclExistsVarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclExistsVarCmdIdx].paramDefs, 
                     method_definitions[XOTclExistsVarCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    CONST char *var = (CONST char *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclExistsVarCmd(interp, object, var);

  }
}

static int
XOTclFinalizeObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclFinalizeObjCmdIdx].paramDefs, 
                     method_definitions[XOTclFinalizeObjCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    

    parseContextRelease(&pc);
    return XOTclFinalizeObjCmd(interp);

  }
}

static int
XOTclForwardCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclForwardCmdIdx].paramDefs, 
                     method_definitions[XOTclForwardCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withPer_object = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *method = (Tcl_Obj *)pc.clientData[2];
    Tcl_Obj *withDefault = (Tcl_Obj *)pc.clientData[3];
    int withEarlybinding = (int )PTR2INT(pc.clientData[4]);
    Tcl_Obj *withMethodprefix = (Tcl_Obj *)pc.clientData[5];
    int withObjscope = (int )PTR2INT(pc.clientData[6]);
    Tcl_Obj *withOnerror = (Tcl_Obj *)pc.clientData[7];
    int withVerbose = (int )PTR2INT(pc.clientData[8]);
    Tcl_Obj *target = (Tcl_Obj *)pc.clientData[9];

    parseContextRelease(&pc);
    return XOTclForwardCmd(interp, object, withPer_object, method, withDefault, withEarlybinding, withMethodprefix, withObjscope, withOnerror, withVerbose, target, objc-pc.lastobjc, objv+pc.lastobjc);

  }
}

static int
XOTclImportvarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclImportvarCmdIdx].paramDefs, 
                     method_definitions[XOTclImportvarCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclImportvarCmd(interp, object, objc-pc.lastobjc, objv+pc.lastobjc);

  }
}

static int
XOTclInterpObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclInterpObjCmdIdx].paramDefs, 
                     method_definitions[XOTclInterpObjCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    CONST char *name = (CONST char *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclInterpObjCmd(interp, name, objc, objv);

  }
}

static int
XOTclInvalidateObjectParameterCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclInvalidateObjectParameterCmdIdx].paramDefs, 
                     method_definitions[XOTclInvalidateObjectParameterCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclClass *class = (XOTclClass *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclInvalidateObjectParameterCmd(interp, class);

  }
}

static int
XOTclIsCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclIsCmdIdx].paramDefs, 
                     method_definitions[XOTclIsCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[0];
    Tcl_Obj *constraint = (Tcl_Obj *)pc.clientData[1];
    Tcl_Obj *withHasmixin = (Tcl_Obj *)pc.clientData[2];
    Tcl_Obj *withType = (Tcl_Obj *)pc.clientData[3];
    Tcl_Obj *arg = (Tcl_Obj *)pc.clientData[4];

    parseContextRelease(&pc);
    return XOTclIsCmd(interp, value, constraint, withHasmixin, withType, arg);

  }
}

static int
XOTclMethodCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclMethodCmdIdx].paramDefs, 
                     method_definitions[XOTclMethodCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withInner_namespace = (int )PTR2INT(pc.clientData[1]);
    int withPer_object = (int )PTR2INT(pc.clientData[2]);
    int withPublic = (int )PTR2INT(pc.clientData[3]);
    Tcl_Obj *name = (Tcl_Obj *)pc.clientData[4];
    Tcl_Obj *args = (Tcl_Obj *)pc.clientData[5];
    Tcl_Obj *body = (Tcl_Obj *)pc.clientData[6];
    Tcl_Obj *withPrecondition = (Tcl_Obj *)pc.clientData[7];
    Tcl_Obj *withPostcondition = (Tcl_Obj *)pc.clientData[8];

    parseContextRelease(&pc);
    return XOTclMethodCmd(interp, object, withInner_namespace, withPer_object, withPublic, name, args, body, withPrecondition, withPostcondition);

  }
}

static int
XOTclMethodPropertyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclMethodPropertyCmdIdx].paramDefs, 
                     method_definitions[XOTclMethodPropertyCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withPer_object = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *methodName = (Tcl_Obj *)pc.clientData[2];
    int methodproperty = (int )PTR2INT(pc.clientData[3]);
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[4];

    parseContextRelease(&pc);
    return XOTclMethodPropertyCmd(interp, object, withPer_object, methodName, methodproperty, value);

  }
}

static int
XOTclMyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclMyCmdIdx].paramDefs, 
                     method_definitions[XOTclMyCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    int withLocal = (int )PTR2INT(pc.clientData[0]);
    Tcl_Obj *method = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclMyCmd(interp, withLocal, method, objc-pc.lastobjc, objv+pc.lastobjc);

  }
}

static int
XOTclNSCopyCmdsStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclNSCopyCmdsIdx].paramDefs, 
                     method_definitions[XOTclNSCopyCmdsIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *fromNs = (Tcl_Obj *)pc.clientData[0];
    Tcl_Obj *toNs = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclNSCopyCmds(interp, fromNs, toNs);

  }
}

static int
XOTclNSCopyVarsStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclNSCopyVarsIdx].paramDefs, 
                     method_definitions[XOTclNSCopyVarsIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *fromNs = (Tcl_Obj *)pc.clientData[0];
    Tcl_Obj *toNs = (Tcl_Obj *)pc.clientData[1];

    parseContextRelease(&pc);
    return XOTclNSCopyVars(interp, fromNs, toNs);

  }
}

static int
XOTclObjectpropertyCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclObjectpropertyCmdIdx].paramDefs, 
                     method_definitions[XOTclObjectpropertyCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *object = (Tcl_Obj *)pc.clientData[0];
    int objectkind = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclObjectpropertyCmd(interp, object, objectkind, value);

  }
}

static int
XOTclParametercheckCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclParametercheckCmdIdx].paramDefs, 
                     method_definitions[XOTclParametercheckCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    int withNocomplain = (int )PTR2INT(pc.clientData[0]);
    Tcl_Obj *param = (Tcl_Obj *)pc.clientData[1];
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclParametercheckCmd(interp, withNocomplain, param, value);

  }
}

static int
XOTclQualifyObjCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclQualifyObjCmdIdx].paramDefs, 
                     method_definitions[XOTclQualifyObjCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    Tcl_Obj *name = (Tcl_Obj *)pc.clientData[0];

    parseContextRelease(&pc);
    return XOTclQualifyObjCmd(interp, name);

  }
}

static int
XOTclRelationCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclRelationCmdIdx].paramDefs, 
                     method_definitions[XOTclRelationCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int relationtype = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclRelationCmd(interp, object, relationtype, value);

  }
}

static int
XOTclSetVarCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclSetVarCmdIdx].paramDefs, 
                     method_definitions[XOTclSetVarCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    Tcl_Obj *variable = (Tcl_Obj *)pc.clientData[1];
    Tcl_Obj *value = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclSetVarCmd(interp, object, variable, value);

  }
}

static int
XOTclSetterCmdStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  parseContext pc;

  if (ArgumentParse(interp, objc, objv, NULL, objv[0], 
                     method_definitions[XOTclSetterCmdIdx].paramDefs, 
                     method_definitions[XOTclSetterCmdIdx].nrParameters, 
                     &pc) != TCL_OK) {
    return TCL_ERROR;
  } else {
    XOTclObject *object = (XOTclObject *)pc.clientData[0];
    int withPer_object = (int )PTR2INT(pc.clientData[1]);
    Tcl_Obj *parameter = (Tcl_Obj *)pc.clientData[2];

    parseContextRelease(&pc);
    return XOTclSetterCmd(interp, object, withPer_object, parameter);

  }
}

static methodDefinition method_definitions[] = {
{"::nsf::cmd::Class::alloc", XOTclCAllocMethodStub, 1, {
  {"name", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Class::create", XOTclCCreateMethodStub, 2, {
  {"name", 1, 0, convertToString},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Class::dealloc", XOTclCDeallocMethodStub, 1, {
  {"object", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Class::filterguard", XOTclCFilterGuardMethodStub, 2, {
  {"filter", 1, 0, convertToString},
  {"guard", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Class::mixinguard", XOTclCMixinGuardMethodStub, 2, {
  {"mixin", 1, 0, convertToString},
  {"guard", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Class::new", XOTclCNewMethodStub, 2, {
  {"-childof", 0, 1, convertToObject},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Class::recreate", XOTclCRecreateMethodStub, 2, {
  {"name", 1, 0, convertToTclobj},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::ClassInfo::filter", XOTclClassInfoFilterMethodStub, 4, {
  {"class", 1, 0, convertToClass},
  {"-guard", 0, 0, convertToString},
  {"-guards", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ClassInfo::forward", XOTclClassInfoForwardMethodStub, 3, {
  {"class", 1, 0, convertToClass},
  {"-definition", 0, 0, convertToString},
  {"name", 0, 0, convertToString}}
},
{"::nsf::cmd::ClassInfo::heritage", XOTclClassInfoHeritageMethodStub, 2, {
  {"class", 1, 0, convertToClass},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ClassInfo::instances", XOTclClassInfoInstancesMethodStub, 3, {
  {"class", 1, 0, convertToClass},
  {"-closure", 0, 0, convertToString},
  {"pattern", 0, 0, convertToObjpattern}}
},
{"::nsf::cmd::ClassInfo::method", XOTclClassInfoMethodMethodStub, 3, {
  {"class", 0, 0, convertToClass},
  {"infomethodsubcmd", 0, 0, convertToInfomethodsubcmd},
  {"name", 0, 0, convertToString}}
},
{"::nsf::cmd::ClassInfo::methods", XOTclClassInfoMethodsMethodStub, 6, {
  {"class", 0, 0, convertToClass},
  {"-methodtype", 0, 1, convertToMethodtype},
  {"-callprotection", 0, 1, convertToCallprotection},
  {"-nomixins", 0, 0, convertToString},
  {"-incontext", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ClassInfo::mixin", XOTclClassInfoMixinMethodStub, 5, {
  {"class", 1, 0, convertToClass},
  {"-closure", 0, 0, convertToString},
  {"-guard", 0, 0, convertToString},
  {"-guards", 0, 0, convertToString},
  {"pattern", 0, 0, convertToObjpattern}}
},
{"::nsf::cmd::ClassInfo::mixinof", XOTclClassInfoMixinOfMethodStub, 4, {
  {"class", 1, 0, convertToClass},
  {"-closure", 0, 0, convertToString},
  {"-scope", 0, 1, convertToScope},
  {"pattern", 0, 0, convertToObjpattern}}
},
{"::nsf::cmd::ClassInfo::slots", XOTclClassInfoSlotsMethodStub, 1, {
  {"class", 1, 0, convertToClass}}
},
{"::nsf::cmd::ClassInfo::subclass", XOTclClassInfoSubclassMethodStub, 3, {
  {"class", 1, 0, convertToClass},
  {"-closure", 0, 0, convertToString},
  {"pattern", 0, 0, convertToObjpattern}}
},
{"::nsf::cmd::ClassInfo::superclass", XOTclClassInfoSuperclassMethodStub, 3, {
  {"class", 1, 0, convertToClass},
  {"-closure", 0, 0, convertToString},
  {"pattern", 0, 0, convertToTclobj}}
},
{"::nsf::cmd::ObjectInfo::callable", XOTclObjInfoCallableMethodStub, 8, {
  {"object", 0, 0, convertToObject},
  {"infocallablesubcmd", 1, 1, convertToInfocallablesubcmd},
  {"-methodtype", 0, 1, convertToMethodtype},
  {"-callprotection", 0, 1, convertToCallprotection},
  {"-application", 0, 0, convertToString},
  {"-nomixins", 0, 0, convertToString},
  {"-incontext", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::children", XOTclObjInfoChildrenMethodStub, 2, {
  {"object", 1, 0, convertToObject},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::class", XOTclObjInfoClassMethodStub, 1, {
  {"object", 1, 0, convertToObject}}
},
{"::nsf::cmd::ObjectInfo::filter", XOTclObjInfoFilterMethodStub, 5, {
  {"object", 1, 0, convertToObject},
  {"-guard", 0, 0, convertToString},
  {"-guards", 0, 0, convertToString},
  {"-order", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::forward", XOTclObjInfoForwardMethodStub, 3, {
  {"object", 1, 0, convertToObject},
  {"-definition", 0, 0, convertToString},
  {"name", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::hasnamespace", XOTclObjInfoHasnamespaceMethodStub, 1, {
  {"object", 1, 0, convertToObject}}
},
{"::nsf::cmd::ObjectInfo::method", XOTclObjInfoMethodMethodStub, 3, {
  {"object", 0, 0, convertToObject},
  {"infomethodsubcmd", 0, 0, convertToInfomethodsubcmd},
  {"name", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::methods", XOTclObjInfoMethodsMethodStub, 6, {
  {"object", 0, 0, convertToObject},
  {"-methodtype", 0, 1, convertToMethodtype},
  {"-callprotection", 0, 1, convertToCallprotection},
  {"-nomixins", 0, 0, convertToString},
  {"-incontext", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::mixin", XOTclObjInfoMixinMethodStub, 5, {
  {"object", 1, 0, convertToObject},
  {"-guard", 0, 0, convertToString},
  {"-guards", 0, 0, convertToString},
  {"-order", 0, 0, convertToString},
  {"pattern", 0, 0, convertToObjpattern}}
},
{"::nsf::cmd::ObjectInfo::parent", XOTclObjInfoParentMethodStub, 1, {
  {"object", 1, 0, convertToObject}}
},
{"::nsf::cmd::ObjectInfo::precedence", XOTclObjInfoPrecedenceMethodStub, 3, {
  {"object", 1, 0, convertToObject},
  {"-intrinsic", 0, 0, convertToString},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::slotobjects", XOTclObjInfoSlotObjectsMethodStub, 2, {
  {"object", 1, 0, convertToObject},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::ObjectInfo::vars", XOTclObjInfoVarsMethodStub, 2, {
  {"object", 1, 0, convertToObject},
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::Object::autoname", XOTclOAutonameMethodStub, 3, {
  {"-instance", 0, 0, convertToString},
  {"-reset", 0, 0, convertToString},
  {"name", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Object::cleanup", XOTclOCleanupMethodStub, 0, {
  }
},
{"::nsf::cmd::Object::configure", XOTclOConfigureMethodStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Object::destroy", XOTclODestroyMethodStub, 0, {
  }
},
{"::nsf::cmd::Object::exists", XOTclOExistsMethodStub, 1, {
  {"var", 1, 0, convertToString}}
},
{"::nsf::cmd::Object::filterguard", XOTclOFilterGuardMethodStub, 2, {
  {"filter", 1, 0, convertToString},
  {"guard", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Object::instvar", XOTclOInstVarMethodStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Object::mixinguard", XOTclOMixinGuardMethodStub, 2, {
  {"mixin", 1, 0, convertToString},
  {"guard", 1, 0, convertToTclobj}}
},
{"::nsf::cmd::Object::noinit", XOTclONoinitMethodStub, 0, {
  }
},
{"::nsf::cmd::Object::requireNamespace", XOTclORequireNamespaceMethodStub, 0, {
  }
},
{"::nsf::cmd::Object::residualargs", XOTclOResidualargsMethodStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Object::uplevel", XOTclOUplevelMethodStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Object::upvar", XOTclOUpvarMethodStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::cmd::Object::vars", XOTclOVarsMethodStub, 1, {
  {"pattern", 0, 0, convertToString}}
},
{"::nsf::cmd::Object::volatile", XOTclOVolatileMethodStub, 0, {
  }
},
{"::nsf::cmd::Object::vwait", XOTclOVwaitMethodStub, 1, {
  {"varname", 1, 0, convertToString}}
},
{"::nsf::alias", XOTclAliasCmdStub, 6, {
  {"object", 0, 0, convertToObject},
  {"-per-object", 0, 0, convertToString},
  {"methodName", 0, 0, convertToString},
  {"-nonleaf", 0, 0, convertToString},
  {"-objscope", 0, 0, convertToString},
  {"cmdName", 1, 0, convertToTclobj}}
},
{"::nsf::assertion", XOTclAssertionCmdStub, 3, {
  {"object", 0, 0, convertToObject},
  {"assertionsubcmd", 1, 0, convertToAssertionsubcmd},
  {"arg", 0, 0, convertToTclobj}}
},
{"::nsf::colon", XOTclColonCmdStub, 1, {
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::configure", XOTclConfigureCmdStub, 2, {
  {"configureoption", 1, 0, convertToConfigureoption},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::createobjectsystem", XOTclCreateObjectSystemCmdStub, 3, {
  {"rootClass", 1, 0, convertToTclobj},
  {"rootMetaClass", 1, 0, convertToTclobj},
  {"systemMethods", 0, 0, convertToTclobj}}
},
{"::nsf::current", XOTclCurrentCmdStub, 1, {
  {"currentoption", 0, 0, convertToCurrentoption}}
},
{"::nsf::deprecated", XOTclDeprecatedCmdStub, 3, {
  {"what", 1, 0, convertToString},
  {"oldCmd", 1, 0, convertToString},
  {"newCmd", 0, 0, convertToString}}
},
{"::nsf::dispatch", XOTclDispatchCmdStub, 4, {
  {"object", 1, 0, convertToObject},
  {"-objscope", 0, 0, convertToString},
  {"command", 1, 0, convertToTclobj},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::existsvar", XOTclExistsVarCmdStub, 2, {
  {"object", 1, 0, convertToObject},
  {"var", 1, 0, convertToString}}
},
{"::nsf::finalize", XOTclFinalizeObjCmdStub, 0, {
  }
},
{"::nsf::forward", XOTclForwardCmdStub, 11, {
  {"object", 1, 0, convertToObject},
  {"-per-object", 0, 0, convertToString},
  {"method", 1, 0, convertToTclobj},
  {"-default", 0, 1, convertToTclobj},
  {"-earlybinding", 0, 0, convertToString},
  {"-methodprefix", 0, 1, convertToTclobj},
  {"-objscope", 0, 0, convertToString},
  {"-onerror", 0, 1, convertToTclobj},
  {"-verbose", 0, 0, convertToString},
  {"target", 0, 0, convertToTclobj},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::importvar", XOTclImportvarCmdStub, 2, {
  {"object", 0, 0, convertToObject},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::interp", XOTclInterpObjCmdStub, 2, {
  {"name", 0, 0, convertToString},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::invalidateobjectparameter", XOTclInvalidateObjectParameterCmdStub, 1, {
  {"class", 0, 0, convertToClass}}
},
{"::nsf::is", XOTclIsCmdStub, 5, {
  {"value", 1, 0, convertToTclobj},
  {"constraint", 1, 0, convertToTclobj},
  {"-hasmixin", 0, 1, convertToTclobj},
  {"-type", 0, 1, convertToTclobj},
  {"arg", 0, 0, convertToTclobj}}
},
{"::nsf::method", XOTclMethodCmdStub, 9, {
  {"object", 1, 0, convertToObject},
  {"-inner-namespace", 0, 0, convertToString},
  {"-per-object", 0, 0, convertToString},
  {"-public", 0, 0, convertToString},
  {"name", 1, 0, convertToTclobj},
  {"args", 1, 0, convertToTclobj},
  {"body", 1, 0, convertToTclobj},
  {"-precondition", 0, 1, convertToTclobj},
  {"-postcondition", 0, 1, convertToTclobj}}
},
{"::nsf::methodproperty", XOTclMethodPropertyCmdStub, 5, {
  {"object", 1, 0, convertToObject},
  {"-per-object", 0, 0, convertToString},
  {"methodName", 1, 0, convertToTclobj},
  {"methodproperty", 1, 0, convertToMethodproperty},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::my", XOTclMyCmdStub, 3, {
  {"-local", 0, 0, convertToString},
  {"method", 1, 0, convertToTclobj},
  {"args", 0, 0, convertToNothing}}
},
{"::nsf::namespace_copycmds", XOTclNSCopyCmdsStub, 2, {
  {"fromNs", 1, 0, convertToTclobj},
  {"toNs", 1, 0, convertToTclobj}}
},
{"::nsf::namespace_copyvars", XOTclNSCopyVarsStub, 2, {
  {"fromNs", 1, 0, convertToTclobj},
  {"toNs", 1, 0, convertToTclobj}}
},
{"::nsf::objectproperty", XOTclObjectpropertyCmdStub, 3, {
  {"object", 1, 0, convertToTclobj},
  {"objectkind", 0, 0, convertToObjectkind},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::parametercheck", XOTclParametercheckCmdStub, 3, {
  {"-nocomplain", 0, 0, convertToString},
  {"param", 0, 0, convertToTclobj},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::__qualify", XOTclQualifyObjCmdStub, 1, {
  {"name", 1, 0, convertToTclobj}}
},
{"::nsf::relation", XOTclRelationCmdStub, 3, {
  {"object", 0, 0, convertToObject},
  {"relationtype", 1, 0, convertToRelationtype},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::setvar", XOTclSetVarCmdStub, 3, {
  {"object", 1, 0, convertToObject},
  {"variable", 1, 0, convertToTclobj},
  {"value", 0, 0, convertToTclobj}}
},
{"::nsf::setter", XOTclSetterCmdStub, 3, {
  {"object", 1, 0, convertToObject},
  {"-per-object", 0, 0, convertToString},
  {"parameter", 0, 0, convertToTclobj}}
},{NULL}
};

