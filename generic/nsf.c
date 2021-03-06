/*
 *  nsf.c --
 *
 *      Basic Machinery of the Next Scripting Framework, a Tcl-based framework
 *      for supporting language-oriented programming.  For details, see
 *      http://next-scripting.org/.
 *
 * Copyright (C) 1999-2015 Gustaf Neumann (a) (b)
 * Copyright (C) 1999-2007 Uwe Zdun (a) (b)
 * Copyright (C) 2007-2008 Martin Matuska (b)
 * Copyright (C) 2010-2015 Stefan Sobernig (b)
 *
 *
 * (a) University of Essen
 *     Specification of Software Systems
 *     Altendorferstrasse 97-101
 *     D-45143 Essen, Germany
 *
 * (b) Vienna University of Economics and Business
 *     Institute of Information Systems and New Media
 *     A-1020, Welthandelsplatz 1
 *     Vienna, Austria
 *
 * This work is licensed under the MIT License
 * http://www.opensource.org/licenses/MIT
 *
 * Copyright:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *
 *  This software is based upon MIT Object Tcl by David Wetherall and
 *  Christopher J. Lindblad, that contains the following copyright
 *  message:
 *
 *   "Copyright 1993 Massachusetts Institute of Technology
 *
 *    Permission to use, copy, modify, distribute, and sell this
 *    software and its documentation for any purpose is hereby granted
 *    without fee, provided that the above copyright notice appear in
 *    all copies and that both that copyright notice and this
 *    permission notice appear in supporting documentation, and that
 *    the name of M.I.T. not be used in advertising or publicity
 *    pertaining to distribution of the software without specific,
 *    written prior permission.  M.I.T. makes no representations about
 *    the suitability of this software for any purpose.  It is
 *    provided "as is" without express or implied warranty."
 */

#define NSF_FORWARD_WITH_ONERROR 1

#define NSF_C 1
#include "nsfInt.h"
#include "nsfAccessInt.h"

#ifdef COMPILE_NSF_STUBS
# if defined(PRE86)
EXTERN NsfStubs nsfStubs;
# else
MODULE_SCOPE const NsfStubs nsfStubs;
# endif
#endif

#ifdef USE_TCL_STUBS
# define Nsf_ExprObjCmd(clientData, interp, objc, objv)        \
  NsfCallCommand(interp, NSF_EXPR, objc, objv)
#else
# define Nsf_ExprObjCmd(clientData, interp, objc, objv)        \
  Tcl_ExprObjCmd(clientData, interp, objc, objv)
#endif

/*
 * Call Stack specific definitions
 */
typedef enum { CALLING_LEVEL, ACTIVE_LEVEL } CallStackLevel;

typedef struct callFrameContext {
  int frameSaved;
  Tcl_CallFrame *framePtr;
  Tcl_CallFrame *varFramePtr;
} callFrameContext;

typedef struct NsfProcContext {
  ClientData oldDeleteData;
  Tcl_CmdDeleteProc *oldDeleteProc;
  NsfParamDefs *paramDefs;
  int checkAlwaysFlag;
} NsfProcContext;

/*
 * TclCmdClientdata is an incomplete type containing the common
 * field(s) of ForwardCmdClientData, AliasCmdClientData and
 * SetterCmdClientData used for filling in at runtime the actual
 * object.
 */
typedef struct TclCmdClientData {
  NsfObject *object;
} TclCmdClientData;

typedef struct SetterCmdClientData {
  NsfObject *object;
  Nsf_Param *paramsPtr;
} SetterCmdClientData;

typedef struct ForwardCmdClientData {
  NsfObject *object;
  Tcl_Obj *cmdName;
  Tcl_ObjCmdProc *objProc;
  ClientData clientData;
  int passthrough;
  int needobjmap;
  int verbose;
  int hasNonposArgs;
  int nr_args;
  Tcl_Obj *args;
  int frame;
#if defined(NSF_FORWARD_WITH_ONERROR)
  Tcl_Obj *onerror;
#endif
  Tcl_Obj *prefix;
  int nr_subcommands;
  Tcl_Obj *subcommands;
} ForwardCmdClientData;

typedef struct AliasCmdClientData {
  NsfObject *object;
  Tcl_Obj *cmdName;
  Tcl_ObjCmdProc *objProc;
  ClientData clientData;
  NsfClass *class;
  Tcl_Interp *interp;
  Tcl_Command aliasedCmd;
  Tcl_Command aliasCmd;
} AliasCmdClientData;

/*
 * When NSF_MEM_COUNT is set, we want to trace as well the mem-count frees
 * associated with the interp. Therefore, we need in this case a special
 * client data structure.
 */
#ifdef NSF_MEM_COUNT
typedef struct NsfNamespaceClientData {
  NsfObject *object;
  Tcl_Namespace *nsPtr;
  Tcl_Interp *interp;
} NsfNamespaceClientData;
#endif

/*
 * Argv parsing specific definitions
 */

#define PARSE_CONTEXT_PREALLOC 20
typedef struct {
  int status;
  ClientData *clientData; /* 4 members pointer to the actual parse context data */
  Tcl_Obj **objv;
  Tcl_Obj **full_objv;    /* contains method as well */
  unsigned int *flags;
  ClientData clientData_static[PARSE_CONTEXT_PREALLOC]; /* 3 members preallocated parse context data */
  Tcl_Obj *objv_static[PARSE_CONTEXT_PREALLOC+1];
  unsigned int flags_static[PARSE_CONTEXT_PREALLOC+1];
  int lastObjc;           /* points to the first "unprocessed" argument */
  int objc;
  int varArgs;            /* does the parameter end with some kind of "args" */
  NsfObject *object;
} ParseContext;

static Nsf_TypeConverter ConvertToNothing, ConvertViaCmd, ConvertToObjpattern;

/*
 * Tcl_Obj Types for Next Scripting Objects
 */

static Tcl_ObjType CONST86
  *Nsf_OT_byteCodeType = NULL,
  *Nsf_OT_tclCmdNameType = NULL,
  *Nsf_OT_listType = NULL,
  *Nsf_OT_doubleType = NULL,
  *Nsf_OT_intType = NULL,
  *Nsf_OT_parsedVarNameType = NULL;

/*
 * Function prototypes
 */

/* Prototypes for method definitions */
static Tcl_ObjCmdProc NsfForwardMethod;
static Tcl_ObjCmdProc NsfObjscopedMethod;
static Tcl_ObjCmdProc NsfSetterMethod;
static Tcl_ObjCmdProc NsfProcAliasMethod;
static Tcl_ObjCmdProc NsfAsmProc;


/* Prototypes for methods called directly when CallDirectly() returns NULL */
static int NsfCAllocMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj)
  nonnull(1) nonnull(2);
static int NsfCAllocMethod_(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj, Tcl_Namespace *parentNsPtr)
  nonnull(1) nonnull(2) nonnull(3);
static int NsfCCreateMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);
static int NsfOCleanupMethod(Tcl_Interp *interp, NsfObject *object)
  nonnull(1) nonnull(2);
static int NsfOConfigureMethod(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *objv0)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);
static int NsfODestroyMethod(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);
static int MethodDispatch(ClientData clientData, Tcl_Interp *interp,
                          int objc, Tcl_Obj *CONST objv[],
                          Tcl_Command cmd, NsfObject *object, NsfClass *cl,
                          const char *methodName, int frameType, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5) nonnull(6) nonnull(8);
static int DispatchDefaultMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *obj, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);
static int DispatchDestroyMethod(Tcl_Interp *interp, NsfObject *object, unsigned int flags)
  nonnull(1) nonnull(2);
static int DispatchUnknownMethod(Tcl_Interp *interp, NsfObject *object,
                                 int objc, Tcl_Obj *CONST objv[], Tcl_Obj *callInfo,
                                 Tcl_Obj *methodObj, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(4) nonnull(6);

NSF_INLINE static int ObjectDispatch(ClientData clientData, Tcl_Interp *interp, int objc,
                                      Tcl_Obj *CONST objv[], unsigned int flags)
  nonnull(1) nonnull(2) nonnull(4);

NSF_INLINE static int ObjectDispatchFinalize(Tcl_Interp *interp, NsfCallStackContent *cscPtr,
                                             int result /*, char *string , const char *methodName*/)
  nonnull(1) nonnull(2);

/* prototypes for object life-cycle management */
static int RecreateObject(Tcl_Interp *interp, NsfClass *cl, NsfObject *object, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);
static void FinalObjectDeletion(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

#if defined(DO_CLEANUP)
static void FreeAllNsfObjectsAndClasses(Tcl_Interp *interp,  NsfCmdList **instances)
  nonnull(1) nonnull(2);
#endif
static void CallStackDestroyObject(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);
static void PrimitiveCDestroy(ClientData clientData) nonnull(1);
static void PrimitiveODestroy(ClientData clientData) nonnull(1);
static void PrimitiveDestroy(ClientData clientData) nonnull(1);

/* prototypes for object and command lookup */
static NsfObject *GetObjectFromString(Tcl_Interp *interp, const char *name)
  nonnull(1) nonnull(2);
static NsfClass *GetClassFromString(Tcl_Interp *interp, const char *name)
  nonnull(1) nonnull(2);
static int GetClassFromObj(Tcl_Interp *interp, register Tcl_Obj *objPtr, NsfClass **clPtr, int withUnknown)
  nonnull(1) nonnull(2) nonnull(3);
/*static NsfObject *GetHiddenObjectFromCmd(Tcl_Interp *interp, Tcl_Command cmdPtr);
static int ReverseLookupCmdFromCmdTable(Tcl_Interp *interp, Tcl_Command searchCmdPtr,
                                        Tcl_HashTable *cmdTablePtr);*/
static void GetAllInstances(Tcl_Interp *interp, NsfCmdList **instances, NsfClass *startClass)
  nonnull(1) nonnull(2) nonnull(3);
NSF_INLINE static Tcl_Command FindMethod(Tcl_Namespace *nsPtr, const char *methodName)
  nonnull(1) nonnull(2);

/* prototypes for namespace specific calls */
static Tcl_Obj *NameInNamespaceObj(const char *name, Tcl_Namespace *ns) nonnull(1) nonnull(2);
static Tcl_Namespace *CallingNameSpace(Tcl_Interp *interp) nonnull(1) returns_nonnull;
NSF_INLINE static Tcl_Command NSFindCommand(Tcl_Interp *interp, const char *name) nonnull(1) nonnull(2);
static Tcl_Namespace *NSGetFreshNamespace(Tcl_Interp *interp, NsfObject *object, const char *name)
  nonnull(1) nonnull(2) nonnull(3);
static Tcl_Namespace *RequireObjNamespace(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);
static int NSDeleteCmd(Tcl_Interp *interp, Tcl_Namespace *nsPtr, const char *methodName)
  nonnull(1) nonnull(2) nonnull(3);
static void NSNamespaceDeleteProc(ClientData clientData) nonnull(1);
static void NSNamespacePreserve(Tcl_Namespace *nsPtr) nonnull(1);
static void NSNamespaceRelease(Tcl_Namespace *nsPtr) nonnull(1);

/* prototypes for filters and mixins */
static void FilterComputeDefined(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);
static void MixinComputeDefined(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);
NSF_INLINE static void GuardAdd(NsfCmdList *filterCL, Tcl_Obj *guardObj) nonnull(1) nonnull(2);
static int GuardCall(NsfObject *object, Tcl_Interp *interp,
                     Tcl_Obj *guardObj, NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2) nonnull(3);
static void GuardDel(NsfCmdList *filterCL) nonnull(1);

/* prototypes for forwarders */
static void ForwardCmdDeleteProc(ClientData clientData) nonnull(1);
static int ForwardProcessOptions(Tcl_Interp *interp, Tcl_Obj *nameObj,
                                 Tcl_Obj *withDefault, int withEarlybinding,
                                 Tcl_Obj *withOnerror, Tcl_Obj *withMethodprefix,
                                 int withFrame, int withVerbose,
                                 Tcl_Obj *target, int objc, Tcl_Obj * CONST objv[],
                                 ForwardCmdClientData **tcdPtr)
  nonnull(1) nonnull(2) nonnull(11);

/* properties of objects and classes */
static int IsRootClass(NsfClass *cl) nonnull(1) pure;
static int IsRootMetaClass(NsfClass *cl) nonnull(1) pure;
static int IsBaseClass(NsfObject *cl) nonnull(1) pure;
static int IsMetaClass(Tcl_Interp *interp, NsfClass *cl, int withMixins) nonnull(1) nonnull(2);
static int IsSubType(NsfClass *subcl, NsfClass *cl) nonnull(1) nonnull(2);
static NsfClass *DefaultSuperClass(Tcl_Interp *interp, NsfClass *cl, NsfClass *mcl, int isMeta)
  nonnull(1) nonnull(2) nonnull(3);


/* prototypes for call stack specific calls */
NSF_INLINE static void CscInit_(NsfCallStackContent *cscPtr, NsfObject *object, NsfClass *cl,
                               Tcl_Command cmd, int frameType, unsigned int flags)
  nonnull(1) nonnull(2);

NSF_INLINE static void CscFinish_(Tcl_Interp *interp, NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2);
NSF_INLINE static void CallStackDoDestroy(Tcl_Interp *interp, NsfObject *object)
  nonnull(1) nonnull(2);

/* prototypes for parameter and argument management */

static int NsfParameterCacheClassInvalidateCmd(Tcl_Interp *interp, NsfClass *cl)
  nonnull(1) nonnull(2);
static int ProcessMethodArguments(ParseContext *pcPtr, Tcl_Interp *interp,
                                  NsfObject *object, int processFlags, NsfParamDefs *paramDefs,
                                  Tcl_Obj *methodNameObj, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(5) nonnull(6) nonnull(8);

static int ParameterCheck(Tcl_Interp *interp, Tcl_Obj *paramObjPtr, Tcl_Obj *valueObj,
                          const char *argNamePrefix, int doCheckArguments,
                          int isNamed, int doConfigureParameter,
                          Nsf_Param **paramPtrPtr)
  nonnull(1) nonnull(2) nonnull(3);
static void ParamDefsRefCountIncr(NsfParamDefs *paramDefs) nonnull(1);
static void ParamDefsRefCountDecr(NsfParamDefs *paramDefs) nonnull(1);
static void ParsedParamFree(NsfParsedParam *parsedParamPtr) nonnull(1);

static int ArgumentParse(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                         NsfObject *obj, Tcl_Obj *procName,
                         Nsf_Param const *paramPtr, int nrParameters, int serial,
                         unsigned int processFlags, ParseContext *pc)
  nonnull(1) nonnull(3) nonnull(5) nonnull(6) nonnull(10);

static int ArgumentCheck(Tcl_Interp *interp, Tcl_Obj *objPtr, struct Nsf_Param const *pPtr, int doCheckArguments,
                         unsigned int *flags, ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(5) nonnull(6) nonnull(7);

static int GetMatchObject(Tcl_Interp *interp, Tcl_Obj *patternObj, Tcl_Obj *origObj,
                          NsfObject **matchObject, const char **pattern)
  nonnull(1) nonnull(4) nonnull(5);
static void NsfProcDeleteProc(ClientData clientData) nonnull(1);

static int NsfParameterCacheObjectInvalidateCmd(Tcl_Interp *interp, NsfObject *object)
  nonnull(1) nonnull(2);

static int GetObjectParameterDefinition(Tcl_Interp *interp, Tcl_Obj *procNameObj,
                                        NsfObject *object, NsfClass *class,
                                        NsfParsedParam *parsedParamPtr)
  nonnull(1) nonnull(2) nonnull(5);

typedef Tcl_Obj *(NsfFormatFunction) _ANSI_ARGS_((Tcl_Interp *interp, Nsf_Param const *paramsPtr,
                                                  NsfObject *contextObject, const char *pattern));

static Tcl_Obj *NsfParamDefsVirtualFormat(Tcl_Interp *interp, Nsf_Param const *pPtr,
                                          NsfObject *contextObject,  const char *pattern,
                                          NsfFormatFunction formatFunction)
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);

static int NsfParamDefsAppendVirtual(Tcl_Interp *interp, Tcl_Obj *listObj,
                                     Nsf_Param const *paramsPtr, NsfObject *contextObject,
                                     const char *pattern,
                                     NsfFormatFunction formatFunction)
  nonnull(1) nonnull(2) nonnull(3) nonnull(6);

/* prototypes for alias management */
static int AliasDelete(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName, int withPer_object)
  nonnull(1) nonnull(2) nonnull(3);
static Tcl_Obj *AliasGet(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName,
                         int withPer_object, int leaveError)
  nonnull(1) nonnull(2) nonnull(3);
static int AliasDeleteObjectReference(Tcl_Interp *interp, Tcl_Command cmd)
  nonnull(1) nonnull(2);
static int NsfMethodAliasCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object,
                             const char *methodName, int withFrame, int withProtection, Tcl_Obj *cmdName)
  nonnull(1) nonnull(2) nonnull(4);

static int AliasRefetch(Tcl_Interp *interp, NsfObject *object, const char *methodName,
                         AliasCmdClientData *tcd)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

NSF_INLINE static Tcl_Command AliasDereference(Tcl_Interp *interp, NsfObject *object,
                                               const char *methodName, Tcl_Command cmd)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

/* prototypes for (class) list handling */
static NsfClasses ** NsfClassListAdd(NsfClasses **firstPtrPtr, NsfClass *cl, ClientData clientData)
  nonnull(1) returns_nonnull;

/* misc prototypes */
static int SetInstVar(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *nameObj, Tcl_Obj *valueObj)
  nonnull(1) nonnull(2) nonnull(3);

static int ListDefinedMethods(Tcl_Interp *interp, NsfObject *object, const char *pattern,
                              int withPer_object, int methodType, int withCallproctection,
                              int withPath)
  nonnull(1) nonnull(2);

static int NextSearchAndInvoke(Tcl_Interp *interp,
                               const char *methodName, int objc, Tcl_Obj *CONST objv[],
                               NsfCallStackContent *cscPtr, int freeArgumentVector)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);

static void CmdListFree(NsfCmdList **cmdList, NsfFreeCmdListClientData *freeFct) nonnull(1);
static void NsfCommandPreserve(Tcl_Command cmd) nonnull(1);
static void NsfCommandRelease(Tcl_Command cmd) nonnull(1);
static Tcl_Command GetOriginalCommand(Tcl_Command cmd) nonnull(1) returns_nonnull;

EXTERN void NsfDStringArgv(Tcl_DString *dsPtr, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(3);

static int MethodSourceMatches(int withSource, NsfClass *cl, NsfObject *object);

static NsfObjectOpt *NsfRequireObjectOpt(NsfObject *object) nonnull(1) returns_nonnull;

static int ObjectSystemsCheckSystemMethod(Tcl_Interp *interp, const char *methodName,
                                          NsfObject *object, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);

#ifdef DO_CLEANUP
static void DeleteNsfProcs(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1);
#endif

#if defined(NSF_WITH_ASSERTIONS)
static void AssertionRemoveProc(NsfAssertionStore *aStore, const char *name)
  nonnull(1) nonnull(2);
#endif

#ifdef DO_FULL_CLEANUP
static void DeleteProcsAndVars(Tcl_Interp *interp, Tcl_Namespace *nsPtr, int withKeepvars)
  nonnull(1) nonnull(2);
#endif

/*
 *----------------------------------------------------------------------
 *
 * NsfErrorContext --
 *
 *      Print the current errorCode and errorInfo to stderr.
 *      This should be used as the last ressort, when e.g. logging fails
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Output to stderr
 *
 *----------------------------------------------------------------------
 */
static void NsfErrorContext(Tcl_Interp *interp, const char *context) nonnull(1) nonnull(2);

static void
NsfErrorContext(Tcl_Interp *interp, const char *context) {
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(interp != NULL);
  nonnull_assert(context != NULL);

  Tcl_DStringInit(dsPtr);
  Tcl_DStringAppend(dsPtr, "puts stderr \"Error in ", -1);
  Tcl_DStringAppend(dsPtr, context, -1);
  Tcl_DStringAppend(dsPtr, ":\n$::errorCode $::errorInfo\"", -1);
  Tcl_EvalEx(interp, Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr), 0);
  Tcl_DStringFree(dsPtr);
}

#if 0
static char *
NsfErrorInfo(Tcl_Interp *interp) {
  Tcl_Obj *valueObj;
  nonnull_assert(interp != NULL);

  valueObj = Tcl_GetVar2Ex(interp, "::errorInfo", NULL, TCL_GLOBAL_ONLY);
  if (valueObj != NULL) {
    return ObjStr(valueObj);
  }
  return NULL;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * NsfDStringEval --
 *
 *      Evaluate the provided Tcl_DString as a Tcl command and output
 *      the error stack in case of a failure.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Output to stderr possible.
 *
 *----------------------------------------------------------------------
 */
static int NsfDStringEval(Tcl_Interp *interp, Tcl_DString *dsPtr, const char *context) nonnull(1) nonnull(2) nonnull(3);

static int
NsfDStringEval(Tcl_Interp *interp, Tcl_DString *dsPtr, const char *context) {
  int result = Tcl_EvalEx(interp, Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr), 0);

  nonnull_assert(interp != NULL);
  nonnull_assert(dsPtr != NULL);
  nonnull_assert(context != NULL);

  if (unlikely(result == TCL_ERROR)) {
    NsfErrorContext(interp, context);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfLog --
 *
 *      Produce a formatted warning by calling an external function
 *      ::nsf::log. It is defined static to allow for inlining.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Output of the warning.
 *
 *----------------------------------------------------------------------
 */

void
NsfLog(Tcl_Interp *interp, int requiredLevel, const char *fmt, ...) {

  nonnull_assert(interp != NULL);
  nonnull_assert(fmt != NULL);

  if (RUNTIME_STATE(interp)->debugLevel >= requiredLevel) {
    Tcl_DString cmdString, ds;
    const char *level;
    va_list ap;

    switch (requiredLevel) {
    case NSF_LOG_DEBUG: level = "Debug"; break;
    case NSF_LOG_NOTICE: level = "Notice"; break;
    default: level = "Warning"; break;
    }

    Tcl_DStringInit(&ds);
    va_start(ap, fmt);
    NsfDStringVPrintf(&ds, fmt, ap);
    va_end(ap);

    Tcl_DStringInit(&cmdString);
    Tcl_DStringAppendElement(&cmdString, "::nsf::log");
    Tcl_DStringAppendElement(&cmdString, level);
    Tcl_DStringAppendElement(&cmdString, Tcl_DStringValue(&ds));
    NsfDStringEval(interp, &cmdString, "log command");
    Tcl_DStringFree(&cmdString);
    Tcl_DStringFree(&ds);
  }
}


/*
 *----------------------------------------------------------------------
 *
 * NsfDeprecatedCmd --
 *
 *      Provide a warning about a deprecated command or method. The
 *      message is produced via calling the external Tcl function
 *      ::nsf::deprecated.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Output of the warning.
 *
 *----------------------------------------------------------------------
 */
static void NsfDeprecatedCmd(Tcl_Interp *interp, const char *what, const char *oldCmd, const char *newCmd)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
NsfDeprecatedCmd(Tcl_Interp *interp, const char *what, const char *oldCmd, const char *newCmd) {
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(interp != NULL);
  nonnull_assert(newCmd != NULL);
  nonnull_assert(what != NULL);
  nonnull_assert(oldCmd != NULL);

  Tcl_DStringInit(dsPtr);
  Tcl_DStringAppendElement(dsPtr, "::nsf::deprecated");
  Tcl_DStringAppendElement(dsPtr, what);
  Tcl_DStringAppendElement(dsPtr, oldCmd);
  Tcl_DStringAppendElement(dsPtr, newCmd);
  NsfDStringEval(interp, dsPtr, "log command");
  Tcl_DStringFree(dsPtr);
}


/***********************************************************************
 * argv parsing
 ***********************************************************************/
/*
 *----------------------------------------------------------------------
 *
 * ParseContextInit --
 *
 *      Initialize a ParseContext with default values and allocate
 *      memory if needed. Every ParseContext has to be initialized
 *      before usage and has to be freed with ParseContextRelease().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Allocate potentially memory.
 *
 *----------------------------------------------------------------------
 */
static void ParseContextInit(ParseContext *pcPtr, int objc, NsfObject *object, Tcl_Obj *procName)
  nonnull(1) nonnull(4);

static void
ParseContextInit(ParseContext *pcPtr, int objc, NsfObject *object, Tcl_Obj *procName) {

  nonnull_assert(pcPtr != NULL);
  nonnull_assert(procName != NULL);

  if (likely(objc < PARSE_CONTEXT_PREALLOC)) {
    /* the single larger memset below .... */
    memset(pcPtr, 0, sizeof(ParseContext));
    /* ... is faster than the two smaller memsets below */
    /* memset(pcPtr->clientData_static, 0, sizeof(ClientData)*(objc));
       memset(pcPtr->objv_static, 0, sizeof(Tcl_Obj *)*(objc+1));*/
    pcPtr->full_objv  = &pcPtr->objv_static[0];
    pcPtr->clientData = &pcPtr->clientData_static[0];
    pcPtr->flags      = &pcPtr->flags_static[0];
  } else {
    pcPtr->full_objv  = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *)*(objc+1));
    pcPtr->flags      = (unsigned int *)ckalloc(sizeof(int)*(objc+1));
    MEM_COUNT_ALLOC("pcPtr.objv", pcPtr->full_objv);
    pcPtr->clientData = (ClientData *)ckalloc(sizeof(ClientData)*objc);
    MEM_COUNT_ALLOC("pcPtr.clientData", pcPtr->clientData);
    /*fprintf(stderr, "ParseContextMalloc %d objc, %p %p\n", objc, pcPtr->full_objv, pcPtr->clientData);*/
    memset(pcPtr->full_objv, 0, sizeof(Tcl_Obj *)*(objc+1));
    memset(pcPtr->flags, 0, sizeof(int)*(objc+1));
    memset(pcPtr->clientData, 0, sizeof(ClientData)*(objc));
    pcPtr->status     = NSF_PC_STATUS_FREE_OBJV|NSF_PC_STATUS_FREE_CD;
    pcPtr->varArgs    = 0;
    pcPtr->objc       = 0;
  }
  pcPtr->objv = &pcPtr->full_objv[1];
  pcPtr->full_objv[0] = procName;
  pcPtr->object = object;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseContextExtendObjv --
 *
 *      Extend Tcl_Obj array at runtime, when more elements are
 *      needed. This function is called to extend an already
 *      initialized ParseContext.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Allocate potentially memory.
 *
 *----------------------------------------------------------------------
 */
static void ParseContextExtendObjv(ParseContext *pcPtr, int from, int elts, Tcl_Obj *CONST source[]) nonnull(1) nonnull(4);

static void
ParseContextExtendObjv(ParseContext *pcPtr, int from, int elts, Tcl_Obj *CONST source[]) {
  int requiredSize = from + elts + 1;

  nonnull_assert(pcPtr != NULL);
  nonnull_assert(source != NULL);

  /*NsfPrintObjv("BEFORE: ", pcPtr->objc, pcPtr->full_objv);*/

  if (unlikely(requiredSize >= PARSE_CONTEXT_PREALLOC)) {
    if (pcPtr->objv == &pcPtr->objv_static[1]) {
      /* realloc from preallocated memory */
      pcPtr->full_objv = (Tcl_Obj **)    ckalloc(sizeof(Tcl_Obj *) * requiredSize);
      pcPtr->flags     = (unsigned int *)ckalloc(sizeof(int) * requiredSize);
      MEM_COUNT_ALLOC("pcPtr.objv", pcPtr->full_objv);
      memcpy(pcPtr->full_objv, &pcPtr->objv_static[0], sizeof(Tcl_Obj *) * PARSE_CONTEXT_PREALLOC);
      memcpy(pcPtr->flags, &pcPtr->flags_static[0], sizeof(int) * PARSE_CONTEXT_PREALLOC);
      /* fprintf(stderr, "ParseContextExtendObjv: extend %p alloc %d new objv=%p pcPtr %p\n",
         pcPtr, requiredSize, pcPtr->full_objv, pcPtr);*/

      pcPtr->status     |= NSF_PC_STATUS_FREE_OBJV;
    } else {
      /* realloc from mallocated memory */
      pcPtr->full_objv = (Tcl_Obj **)    ckrealloc((char *)pcPtr->full_objv, sizeof(Tcl_Obj *) * requiredSize);
      pcPtr->flags     = (unsigned int *)ckrealloc((char *)pcPtr->flags,     sizeof(int) * requiredSize);
      /*fprintf(stderr, "ParseContextExtendObjv: extend %p realloc %d  new objv=%p pcPtr %p\n",
        pcPtr, requiredSize, pcPtr->full_objv, pcPtr);*/
    }
    pcPtr->objv = &pcPtr->full_objv[1];
  }

  memcpy(pcPtr->objv + from, source, sizeof(Tcl_Obj *) * elts);
  memset(pcPtr->flags + from, 0, sizeof(int) * elts);
  pcPtr->objc += elts;

  /*NsfPrintObjv("AFTER:  ", pcPtr->objc, pcPtr->full_objv);*/
}

/*
 *----------------------------------------------------------------------
 *
 * ParseContextRelease --
 *
 *      Release (and potentially free) the content of a
 *      ParseContext. This function is the counterpart of
 *      ParseContextInit(),
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Free potentially memory.
 *
 *----------------------------------------------------------------------
 */
static void ParseContextRelease(ParseContext *pcPtr) nonnull(1);

static void
ParseContextRelease(ParseContext *pcPtr) {
  int status;

  nonnull_assert(pcPtr != NULL);

  status = pcPtr->status;

  /*fprintf(stderr, "ParseContextRelease %p status %.6x %d elements\n",
    pcPtr, status, pcPtr->objc);*/

#if !defined(NDEBUG)
  {
    /*
     * Perform a general consistency check: although the contents of the parse
     * context are at release time sometimes only partially initialized, the
     * following holds true for ensuring correct release of Tcl_Objs:
     *
     *  1) if one of the objv-flags has NSF_PC_MUST_DECR set,
     *     then the status flag NSF_PC_STATUS_MUST_DECR has to
     *     be set as well.
     *
     *  2) if objc > 0 then for all objv entries having a flag
     *     different from 0  must have a
     *     TCL_OBJ in the vector.
     *
     *  3) for preallocated objvs, all elements of the objv
     *     after the argument vector must be 0 or
     *     NSF_PC_IS_DEFAULT (sanity check)
     */
    /*
     * (1) make sure, that the status correctly reflects MUST_DECR
     */
    int i;
    if (status == 0 || (status & NSF_PC_STATUS_MUST_DECR) == 0) {
      for (i = 0; i < pcPtr->objc - 1; i++) {
        assert((pcPtr->flags[i] & NSF_PC_MUST_DECR) == 0);
      }
    }

    /*
     * (2) make sure, Tcl_Objs are set when needed for reclaiming memory
     */
    if (pcPtr->objc > 0) {
      /*fprintf(stderr, "%s ", ObjStr(pcPtr->full_objv[0]));*/
      for (i = 0; i < pcPtr->objc; i++) {
        if (pcPtr->flags[i]) {
          assert(pcPtr->objv[i]);
          /*fprintf(stderr, "[%d]%s %.6x ", i, ObjStr(pcPtr->objv[i]), pcPtr->flags[i]);*/
        }
      }
    }
    /*
     * (3) All later flags must be empty or DEFAULT
     */
    if (pcPtr->full_objv == &pcPtr->objv_static[0] && pcPtr->objc > 0) {
      for (i = pcPtr->objc; i < PARSE_CONTEXT_PREALLOC; i++) {
        assert(pcPtr->flags[i] == 0 || pcPtr->flags[i] == NSF_PC_IS_DEFAULT);
      }
    }
  }
#endif

  if (unlikely(status)) {
    if (status & NSF_PC_STATUS_MUST_DECR) {
      int i;
      /*fprintf(stderr, "ParseContextRelease %p loop from 0 to %d\n", pcPtr, pcPtr->objc-1);*/
      for (i = 0; i < pcPtr->objc; i++) {
        /*fprintf(stderr, "ParseContextRelease %p check [%d] obj %p flags %.6x & %p\n",
                pcPtr, i, pcPtr->objv[i],
                pcPtr->flags[i], &(pcPtr->flags[i]));*/
        if (pcPtr->flags[i] & NSF_PC_MUST_DECR) {
          assert(pcPtr->objv[i]);
          assert(pcPtr->objv[i]->refCount > 0);
          /*fprintf(stderr, "... decr ref count on %p\n", pcPtr->objv[i]);*/
          DECR_REF_COUNT2("valueObj", pcPtr->objv[i]);
        }
      }
    }
    /*
     * Objv can be separately extended; also flags are extend when this
     * happens.
     */
    if (unlikely(status & NSF_PC_STATUS_FREE_OBJV)) {
      /*fprintf(stderr, "ParseContextRelease %p free %p %p\n",
        pcPtr, pcPtr->full_objv, pcPtr->clientData);*/
      MEM_COUNT_FREE("pcPtr.objv", pcPtr->full_objv);
      ckfree((char *)pcPtr->full_objv);
      ckfree((char *)pcPtr->flags);
    }
    /*
     * If the parameter definition was extended at creation time also
     * clientData is extended.
     */
    if (status & NSF_PC_STATUS_FREE_CD) {
      /*fprintf(stderr, "free client-data for %p\n", pcPtr);*/
      MEM_COUNT_FREE("pcPtr.clientData", pcPtr->clientData);
      ckfree((char *)pcPtr->clientData);
    }
  }
}

/*
 *----------------------------------------------------------------------
 *
 * CallMethod --
 *
 *      Call a Next Scripting method. clientData has to contain the object, on
 *      which the method is to be dispatched, methodDobj denotes the method,
 *      objc ( which has to be >=2) and objv denotes the argument vector.
 *
 * Results:
 *      Tcl return code
 *
 * Side effects:
 *      potentially via the called method.
 *
 *----------------------------------------------------------------------
 */
/*
 * call an Next Scripting method
 */
static int
CallMethod(ClientData clientData, Tcl_Interp *interp, Tcl_Obj *methodObj,
           int objc, Tcl_Obj *CONST objv[], unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);

static int
CallMethod(ClientData clientData, Tcl_Interp *interp, Tcl_Obj *methodObj,
           int objc, Tcl_Obj *CONST objv[], unsigned int flags) {
  NsfObject *object;
  int result;
  ALLOC_ON_STACK(Tcl_Obj*, objc, tov);

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(methodObj != NULL);
  assert(objc > 1);

  object = (NsfObject *) clientData;
  tov[0] = object->cmdName;
  tov[1] = methodObj;

  if (likely(objc > 2)) {
    memcpy(tov+2, objv, sizeof(Tcl_Obj *)*(objc - 2));
  }

  /*fprintf(stderr, "%%%% CallMethod cmdName=%s, method=%s, objc=%d\n",
    ObjStr(tov[0]), ObjStr(tov[1]), objc);
    {int i; fprintf(stderr, "\t CALL: %s ", ObjStr(methodObj));for(i = 0; i < objc-2; i++) {
    fprintf(stderr, "%s ", ObjStr(objv[i]));} fprintf(stderr, "\n");}*/

  result = ObjectDispatch(clientData, interp, objc, tov, flags);

  FREE_ON_STACK(Tcl_Obj*, tov);
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfCallMethodWithArgs --
 *
 *      Call method (passed in methodObj) on the object, with the often
 *      provided arg1 and the optional remaining args (passed vis objv).  This
 *      way, we save the memcpy in case no argument or an single argument are
 *      provided (common cases).
 *
 * Results:
 *      Tcl result.
 *
 * Side effects:
 *      Called method might side effect.
 *
 *----------------------------------------------------------------------
 */

int NsfCallMethodWithArgs(Tcl_Interp *interp, Nsf_Object *object, Tcl_Obj *methodObj,
                      Tcl_Obj *arg1, int givenObjc, Tcl_Obj *CONST objv[], unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);

int
NsfCallMethodWithArgs(Tcl_Interp *interp, Nsf_Object *object, Tcl_Obj *methodObj,
                      Tcl_Obj *arg1, int givenObjc, Tcl_Obj *CONST objv[], unsigned int flags) {
  int objc = givenObjc + 2;
  int result;
  ALLOC_ON_STACK(Tcl_Obj*, objc, tov);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  assert(ISOBJ(methodObj));
  assert(objc > 1);

  tov[0] = object->cmdName;
  tov[1] = methodObj;
  if (objc > 2) {
    tov[2] = arg1;
  }
  if (objc > 3) {
    memcpy(tov+3, objv, sizeof(Tcl_Obj *)*(objc-3));
  }

  /*fprintf(stderr, "%%%% CallMethodWithArgs cmdName=%s, method=%s, arg1 %s objc=%d\n",
          ObjStr(tov[0]), ObjStr(tov[1]), (objc > 2) ? ObjStr(tov[2]) : "",  objc);*/

  result = ObjectDispatch(object, interp, objc, tov, flags);

  FREE_ON_STACK(Tcl_Obj*, tov);
  return result;
}

/*
 * Support for variable hash tables
 */
static NSF_INLINE Var *VarHashCreateVar(TclVarHashTable *tablePtr, Tcl_Obj *key, int *newPtr) nonnull(1) nonnull(2);

static NSF_INLINE Var *
VarHashCreateVar(TclVarHashTable *tablePtr, Tcl_Obj *key, int *newPtr) {
  Var *varPtr = NULL;
  Tcl_HashEntry *hPtr;

  nonnull_assert(tablePtr != NULL);
  nonnull_assert(key != NULL);

  hPtr = Tcl_CreateHashEntry((Tcl_HashTable *) tablePtr,
                             (char *) key, newPtr);
  if (likely(hPtr != NULL)) {
    varPtr = TclVarHashGetValue(hPtr);
  }
  return varPtr;
}

static NSF_INLINE TclVarHashTable *
VarHashTableCreate() {
  TclVarHashTable *varTablePtr = (TclVarHashTable *) ckalloc(sizeof(TclVarHashTable));

  TclInitVarHashTable(varTablePtr, NULL);
  return varTablePtr;
}

#include "nsfCmdPtr.c"
#include "nsfStack.c"

/***********************************************************************
 * Value added replacements of Tcl functions
 ***********************************************************************/
/*
 *----------------------------------------------------------------------
 * Nsf_NextHashEntry --
 *
 *    Function very similar to Tcl_NextHashEntry. If during the iteration of
 *    hash entries some of these entries are removed, Tcl_NextHashEntry() can
 *    lead to a valid looking but invalid hPtr, when the next entry was
 *    already deleted. This seem to occur only, when there are more than 12
 *    hash entries in the table (multiple buckets).  Therefore, we use
 *    numEntries to check, if it is sensible to return a an hash entry. We can
 *    trigger refetch of the hSrchPtr, when the number of expected entries
 *    differs from the numbers of the actual entries.
 *
 * Results:
 *    Hash Entry or NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_HashEntry * Nsf_NextHashEntry(Tcl_HashTable *tablePtr, int expected, Tcl_HashSearch *hSrchPtr)
  nonnull(1) nonnull(3);

static Tcl_HashEntry *
Nsf_NextHashEntry(Tcl_HashTable *tablePtr, int expected, Tcl_HashSearch *hSrchPtr) {

  nonnull_assert(tablePtr != NULL);
  nonnull_assert(hSrchPtr != NULL);

  /*fprintf(stderr, "Nsf_NextHashEntry %p expected %d numEntries %d\n",
    tablePtr, expected, tablePtr->numEntries);*/
  if (tablePtr->numEntries < 1) {
    return NULL;
  } else if (tablePtr->numEntries != expected) {
    return Tcl_FirstHashEntry(tablePtr, hSrchPtr);
  } else {
    return Tcl_NextHashEntry(hSrchPtr);
  }
}

/*
 *----------------------------------------------------------------------
 * NsfCommandPreserve --
 *
 *    Increment Tcl's command refCount
 *
 * Results:
 *    void
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void
NsfCommandPreserve(Tcl_Command cmd) {

  nonnull_assert(cmd != NULL);

  Tcl_Command_refCount(cmd)++;
  MEM_COUNT_ALLOC("command.refCount", cmd);
}

/*
 *----------------------------------------------------------------------
 * NsfCommandRelease --
 *
 *    Decrement Tcl command refCount and free it if necessary
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Free pot. memory
 *
 *----------------------------------------------------------------------
 */
static void
NsfCommandRelease(Tcl_Command cmd) {

  nonnull_assert(cmd != NULL);

  /*fprintf(stderr,"NsfCommandRelease %p\n", cmd);*/
  MEM_COUNT_FREE("command.refCount", cmd);
  TclCleanupCommandMacro((Command *)cmd);
}

/***********************************************************************
 * EXTERN callable routines for the preliminary C interface
 ***********************************************************************/
Nsf_Object * NsfGetSelfObj(Tcl_Interp *interp) nonnull(1);
Nsf_Object * NsfGetObject(Tcl_Interp *interp, const char *name) nonnull(1) nonnull(2);
Nsf_Class * NsfGetClass(Tcl_Interp *interp, const char *name) nonnull(1) nonnull(2);
Nsf_Class * NsfIsClass(Tcl_Interp *interp, ClientData clientData) nonnull(1) nonnull(2);
void NsfRequireObjNamespace(Tcl_Interp *interp, Nsf_Object *object) nonnull(1) nonnull(2);
Tcl_Obj * Nsf_ObjSetVar2(Nsf_Object *object, Tcl_Interp *interp, Tcl_Obj *name1, Tcl_Obj *name2,
                         Tcl_Obj *valueObj, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);
Tcl_Obj * Nsf_ObjGetVar2(Nsf_Object *object, Tcl_Interp *interp, Tcl_Obj *name1, Tcl_Obj *name2, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);
int NsfCreate(Tcl_Interp *interp, Nsf_Class *class, Tcl_Obj *nameObj, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);
int NsfDeleteObject(Tcl_Interp *interp, Nsf_Object *object) nonnull(1) nonnull(2);
int NsfRemoveObjectMethod(Tcl_Interp *interp, Nsf_Object *object1, const char *methodName)
  nonnull(1) nonnull(2) nonnull(3);
int NsfRemoveClassMethod(Tcl_Interp *interp, Nsf_Class *class, const char *methodName)
  nonnull(1) nonnull(2) nonnull(3);
int Nsf_UnsetVar2(Nsf_Object *object1, Tcl_Interp *interp,
                  const char *name1, const char *name2, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(4);

void NsfSetObjClientData(Tcl_Interp *interp, Nsf_Object *object, ClientData data) nonnull(1) nonnull(2);
ClientData NsfGetObjClientData(Tcl_Interp *interp, Nsf_Object *object) nonnull(1) nonnull(2);
void NsfSetClassClientData(Tcl_Interp *interp, Nsf_Class *cl, ClientData data) nonnull(1) nonnull(2);
ClientData NsfGetClassClientData(Tcl_Interp *interp, Nsf_Class *cl) nonnull(1) nonnull(2);

Nsf_Object *
NsfGetSelfObj(Tcl_Interp *interp) {
  nonnull_assert(interp != NULL);
  return (Nsf_Object *) GetSelfObj(interp);
}

Nsf_Object *
NsfGetObject(Tcl_Interp *interp, const char *name) {
  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  return (Nsf_Object *) GetObjectFromString(interp, name);
}

Nsf_Class *
NsfGetClass(Tcl_Interp *interp, const char *name) {
  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  return (Nsf_Class *)GetClassFromString(interp, name);
}

Nsf_Class *
NsfIsClass(Tcl_Interp *UNUSED(interp), ClientData clientData) {

  nonnull_assert(clientData != NULL);

  if (NsfObjectIsClass((NsfObject *)clientData)) {
    return (Nsf_Class *) clientData;
  }
  return NULL;
}

void
NsfRequireObjNamespace(Tcl_Interp *interp, Nsf_Object *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  RequireObjNamespace(interp, (NsfObject *) object);
}

Tcl_Obj *
Nsf_ObjSetVar2(Nsf_Object *object, Tcl_Interp *interp, Tcl_Obj *name1, Tcl_Obj *name2,
                 Tcl_Obj *valueObj, unsigned int flags) {
  Tcl_Obj *result;
  CallFrame frame, *framePtr = &frame;

  nonnull_assert(object != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(name1 != NULL);
  nonnull_assert(valueObj != NULL);

  Nsf_PushFrameObj(interp, (NsfObject *)object, framePtr);
  if (((NsfObject *)object)->nsPtr) {
    flags |= TCL_NAMESPACE_ONLY;
  }
  result = Tcl_ObjSetVar2(interp, name1, name2, valueObj, flags);
  Nsf_PopFrameObj(interp, framePtr);
  return result;
}


Tcl_Obj *
Nsf_ObjGetVar2(Nsf_Object *object, Tcl_Interp *interp, Tcl_Obj *name1, Tcl_Obj *name2,
               unsigned int flags) {
  Tcl_Obj *result;
  CallFrame frame, *framePtr = &frame;

  nonnull_assert(object != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(name1 != NULL);

  Nsf_PushFrameObj(interp, (NsfObject *)object, framePtr);
  if (((NsfObject *)object)->nsPtr) {
    flags |= TCL_NAMESPACE_ONLY;
  }
  result = Tcl_ObjGetVar2(interp, name1, name2, flags);
  Nsf_PopFrameObj(interp, framePtr);

  return result;
}

int
Nsf_UnsetVar2(Nsf_Object *object1, Tcl_Interp *interp,
                   const char *name1, const char *name2, unsigned int flags) {
  CallFrame frame, *framePtr = &frame;
  NsfObject *object;
  int result;

  nonnull_assert(object1 != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(name1 != NULL);
  nonnull_assert(name2 != NULL);

  object = (NsfObject *) object1;
  Nsf_PushFrameObj(interp, object, framePtr);
  if (object->nsPtr != NULL) {
    flags |= TCL_NAMESPACE_ONLY;
  }
  result = Tcl_UnsetVar2(interp, name1, name2, flags);
  Nsf_PopFrameObj(interp, framePtr);
  return result;
}

int
NsfCreate(Tcl_Interp *interp, Nsf_Class *class, Tcl_Obj *nameObj,
          int objc, Tcl_Obj *CONST objv[]) {
  NsfClass *cl = (NsfClass *) class;
  int result;
  ALLOC_ON_STACK(Tcl_Obj *, objc+2, ov);

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(objv != NULL);

  INCR_REF_COUNT2("nameObj", nameObj);

  ov[0] = NULL;
  ov[1] = nameObj;
  if (objc > 0) {
    memcpy(ov+2, objv, sizeof(Tcl_Obj *)*objc);
  }
  result = NsfCCreateMethod(interp, cl, nameObj, objc+2, ov);

  FREE_ON_STACK(Tcl_Obj*, ov);
  DECR_REF_COUNT2("nameObj", nameObj);

  return result;
}

int
NsfDeleteObject(Tcl_Interp *interp, Nsf_Object *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return DispatchDestroyMethod(interp, (NsfObject *)object, 0);
}

int
NsfRemoveObjectMethod(Tcl_Interp *interp, Nsf_Object *object1, const char *methodName) {
  NsfObject *object;

  nonnull_assert(interp != NULL);
  nonnull_assert(object1 != NULL);
  nonnull_assert(methodName != NULL);

  object = (NsfObject *) object1;
  /*fprintf(stderr, "... NsfRemoveObjectMethod %s %s\n", ObjectName(object), methodName);*/

  NsfObjectMethodEpochIncr("NsfRemoveObjectMethod");
  AliasDelete(interp, object->cmdName, methodName, 1);

#if defined(NSF_WITH_ASSERTIONS)
  if (object->opt != NULL && object->opt->assertions != NULL) {
    AssertionRemoveProc(object->opt->assertions, methodName);
  }
#endif

  if (object->nsPtr != NULL) {
    int rc = NSDeleteCmd(interp, object->nsPtr, methodName);
    if (rc < 0) {
      return NsfPrintError(interp, "%s: cannot delete object specific method '%s'",
                           ObjectName(object), methodName);
    }
  }
  return TCL_OK;
}

int
NsfRemoveClassMethod(Tcl_Interp *interp, Nsf_Class *class, const char *methodName) {
  NsfClass *cl;
  int rc;
#if defined(NSF_WITH_ASSERTIONS)
  NsfClassOpt *opt;
#endif

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(methodName != NULL);

  cl = (NsfClass *)class;
  /*fprintf(stderr, "... NsfRemoveClassMethod %s %s\n", ClassName(class), methodName);*/

  NsfInstanceMethodEpochIncr("NsfRemoveClassMethod");
  AliasDelete(interp, class->object.cmdName, methodName, 0);

#if defined(NSF_WITH_ASSERTIONS)
  opt = cl->opt;
  if (opt != NULL && opt->assertions != NULL) {
    AssertionRemoveProc(opt->assertions, methodName);
  }
#endif

  rc = NSDeleteCmd(interp, cl->nsPtr, methodName);
  if (rc < 0) {
    return NsfPrintError(interp, "%s: cannot delete method '%s'", ClassName(cl), methodName);
  }
  return TCL_OK;
}

/*
 * obj/cl ClientData setter/getter
 */

void
NsfSetObjClientData(Tcl_Interp *interp, Nsf_Object *object, ClientData data) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(data != NULL);

  NsfRequireObjectOpt((NsfObject *) object) -> clientData = data;
}

ClientData
NsfGetObjClientData(Tcl_Interp *interp, Nsf_Object *object) {
  NsfObject *object_;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  object_ = (NsfObject *) object;
  return (object_->opt != NULL) ? object_->opt->clientData : NULL;
}

void
NsfSetClassClientData(Tcl_Interp *interp, Nsf_Class *cl, ClientData data) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  NsfRequireClassOpt((NsfClass *)cl) -> clientData = data;
}

ClientData
NsfGetClassClientData(Tcl_Interp *interp, Nsf_Class *cl) {
  NsfClass *cl_;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  cl_ = (NsfClass *) cl;
  return (cl_->opt != NULL) ? cl_->opt->clientData : NULL;
}

/***********************************************************************
 * Utility functions
 ***********************************************************************/

#if defined(NSFOBJ_TRACE)
void ObjTrace(char *string, NsfObject *object) nonnull(1) nonnull(2);

void
ObjTrace(char *string, NsfObject *object) {

  nonnull_assert(string != NULL);
  nonnull_assert(object != NULL);

  fprintf(stderr, "--- %s tcl %p %s (%d %p) nsf %p (%d) %s \n", string,
          object->cmdName, (object->cmdName->typePtr != NULL) ? object->cmdName->typePtr->name : "NULL",
          object->cmdName->refCount, object->cmdName->internalRep.twoPtrValue.ptr1,
          object, object->refCount, ObjectName(object));
}
#else
# define ObjTrace(a, b)
#endif


/*
 *----------------------------------------------------------------------
 * NSTail --
 *
 *    Return the namespace tail of a name.
 *
 * Results:
 *    String.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
/* search for tail of name */
static const char * NSTail(const char *string) nonnull(1);

static const char *
NSTail(const char *string) {
  register const char *p;

  nonnull_assert(string != NULL);

  p = string + strlen(string);
  while (p > string) {
    if (unlikely(*p == ':' && *(p-1) == ':')) {
      return p+1;
    }
    p--;
  }
  return string;
}

/*
 *----------------------------------------------------------------------
 * IsClassNsName --
 *
 *    Check, if the provided string starts with the prefix of the
 *    classes namespace.
 *
 * Results:
 *    Boolean.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int IsClassNsName(const char *string, const char **cont) nonnull(1);

NSF_INLINE static int
IsClassNsName(const char *string, const char **cont) {

  nonnull_assert(string != NULL);

  if (*string == ':' && strncmp((string), "::nsf::classes", 14) == 0) {
    if (cont != NULL) {*cont = string + 14;}
    return 1;
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetObjectFromNsName --
 *
 *    Get object or class from a fully qualified cmd name, such as
 *    e.g. ::nsf::classes::X
 *
 * Results:
 *    NsfObject and *fromClasses
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static NsfObject * GetObjectFromNsName(Tcl_Interp *interp, const char *string, int *fromClassNS)
  nonnull(1) nonnull(2) nonnull(3);

NSF_INLINE static NsfObject *
GetObjectFromNsName(Tcl_Interp *interp, const char *string, int *fromClassNS) {
  const char *className;

  nonnull_assert(interp != NULL);
  nonnull_assert(string != NULL);
  nonnull_assert(fromClassNS != NULL);

  if (IsClassNsName(string, &className)) {
    *fromClassNS = 1;
    return (NsfObject *)GetClassFromString(interp, className);
  } else {
    *fromClassNS = 0;
    return GetObjectFromString(interp, string);
  }
}

/*
 *----------------------------------------------------------------------
 * DStringAppendQualName --
 *
 *    Append to initialized DString the name of the namespace followed
 *    by a simple name (methodName, cmdName).
 *
 * Results:
 *    String pointing to DString value.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static char *DStringAppendQualName(Tcl_DString *dsPtr, Tcl_Namespace *nsPtr, const char *name)
  nonnull(1) nonnull(2) nonnull(3);

static char *
DStringAppendQualName(Tcl_DString *dsPtr, Tcl_Namespace *nsPtr, const char *name) {
  int oldLength = Tcl_DStringLength(dsPtr);

  nonnull_assert(dsPtr != NULL);
  nonnull_assert(nsPtr != NULL);
  nonnull_assert(name != NULL);

  Tcl_DStringAppend(dsPtr, nsPtr->fullName, -1);
  if (Tcl_DStringLength(dsPtr) > (oldLength + 2)) {
    Tcl_DStringAppend(dsPtr, "::", 2);
  }
  Tcl_DStringAppend(dsPtr, name, -1);
  return Tcl_DStringValue(dsPtr);
}

/*
 *----------------------------------------------------------------------
 * NsfCleanupObject --
 *
 *    Delete an object physically (performing ckfree()) when its refCount
 *    reaches 0
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees memory.
 *
 *----------------------------------------------------------------------
 */
void
NsfCleanupObject_(NsfObject *object) {

  nonnull_assert(object != NULL);

  NsfObjectRefCountDecr(object);
  /*fprintf(stderr, "NsfCleanupObject obj refCount of %p after decr %d id %p interp %p flags %.6x\n",
    object, object->refCount, object->id, object->teardown, object->flags);*/

  if (unlikely(object->refCount <= 0)) {
    /*fprintf(stderr, "NsfCleanupObject %p ref-count %d\n", object, object->refCount);*/
    assert(object->refCount == 0);
    assert((object->flags & NSF_DELETED) != 0u);

    /*
     * During FinalObjectDeletion(), object->teardown is NULL, we cannot access
     * the object and class names anymore.
     */
    if (object->teardown && NSF_DTRACE_OBJECT_FREE_ENABLED()) {
      NSF_DTRACE_OBJECT_FREE(ObjectName(object), ClassName(object->cl));
    }

    MEM_COUNT_FREE("NsfObject/NsfClass", object);
#if defined(NSFOBJ_TRACE)
    fprintf(stderr, "CKFREE Object %p refCount=%d\n", object, object->refCount);
#endif
#if !defined(NDEBUG)
    memset(object, 0, sizeof(NsfObject));
#endif
    ckfree((char *) object);
  }
}


/*
 *  Tcl_Obj functions for objects
 */

/*
 *----------------------------------------------------------------------
 * TclObjIsNsfObject --
 *
 *    Check, if the provided Tcl_Obj is bound to a nsf object. If so, return
 *    the NsfObject in the third argument.
 *
 * Results:
 *    True or false,
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int TclObjIsNsfObject(Tcl_Interp *interp, Tcl_Obj *objPtr, NsfObject **objectPtr)
  nonnull(1) nonnull(2) nonnull(3);

static int
TclObjIsNsfObject(Tcl_Interp *interp, Tcl_Obj *objPtr, NsfObject **objectPtr) {
  Tcl_ObjType CONST86 *cmdType;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(objectPtr != NULL);

  cmdType = objPtr->typePtr;
  if (cmdType == Nsf_OT_tclCmdNameType) {
    Tcl_Command cmd = Tcl_GetCommandFromObj(interp, objPtr);
    if (likely(cmd != NULL)) {
      NsfObject *object = NsfGetObjectFromCmdPtr(cmd);
      if (object != NULL) {
        *objectPtr = object;
        return 1;
      }
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetObjectFromObj --
 *
 *    Lookup an Next Scripting object from the given objPtr, preferably from
 *    an object of type "cmdName". On success the NsfObject is returned in the
 *    third argument. The objPtr might be converted by this function.
 *
 * Results:
 *    True or false,
 *
 * Side effects:
 *    object type of objPtr might be changed
 *
 *----------------------------------------------------------------------
 */
static int GetObjectFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, NsfObject **objectPtr)
  nonnull(1) nonnull(2) nonnull(3);

static int
GetObjectFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, NsfObject **objectPtr) {
  NsfObject *object;
  const char *string;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(objectPtr != NULL);

  /*fprintf(stderr, "GetObjectFromObj obj %p %s is of type %s\n",
    objPtr, ObjStr(objPtr), (objPtr->typePtr != NULL) ? objPtr->typePtr->name : "(null)");*/

  /* in case, objPtr was not of type cmdName, try to convert */
  cmd = Tcl_GetCommandFromObj(interp, objPtr);
  /*fprintf(stderr, "GetObjectFromObj obj %s => cmd=%p (%d)\n",
    ObjStr(objPtr), cmd, (cmd != NULL) ? Tcl_Command_refCount(cmd):-1);*/
  if (cmd != NULL) {
    NsfObject *object = NsfGetObjectFromCmdPtr(cmd);

    /*fprintf(stderr, "GetObjectFromObj obj %s, o is %p objProc %p NsfObjDispatch %p\n", ObjStr(objPtr),
      object, Tcl_Command_objProc(cmd), NsfObjDispatch);*/
    if (likely(object != NULL)) {
      *objectPtr = object;
      return TCL_OK;
    }
  }

  /*fprintf(stderr, "GetObjectFromObj convertFromAny for %s type %p %s\n", ObjStr(objPtr),
    objPtr->typePtr, (objPtr->typePtr != NULL) ? objPtr->typePtr->name : "(none)");*/

  /* In case, we have to revolve via the CallingNameSpace (i.e. the
   * argument is not fully qualified), we retry here.
   */
  string = ObjStr(objPtr);
  if (isAbsolutePath(string)) {
    object = NULL;
  } else {
    Tcl_Obj *tmpName = NameInNamespaceObj(string, CallingNameSpace(interp));
    const char *nsString = ObjStr(tmpName);

    INCR_REF_COUNT(tmpName);
    object = GetObjectFromString(interp, nsString);
    /* fprintf(stderr, " RETRY, string '%s' returned %p\n", nsString, object);*/
    DECR_REF_COUNT(tmpName);
  }

  if (likely(object != NULL)) {
    *objectPtr = object;
    return TCL_OK;
  }

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 * NsfCallObjectUnknownHandler --
 *
 *    Call ::nsf::object::unknown; this function is typically called, when an unknown
 *    object or class is passed as an argument.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    Called handler might side effect.
 *
 *----------------------------------------------------------------------
 */

static int NsfCallObjectUnknownHandler(Tcl_Interp *interp, Tcl_Obj *nameObj) nonnull(1) nonnull(2);

static int
NsfCallObjectUnknownHandler(Tcl_Interp *interp, Tcl_Obj *nameObj) {
  int result;
  Tcl_Obj *ov[3];

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);

  /*fprintf(stderr, "try ::nsf::object::unknown for '%s'\n", ObjStr(nameObj));*/

  ov[0] = NsfGlobalObjs[NSF_OBJECT_UNKNOWN_HANDLER];
  ov[1] = nameObj;

  INCR_REF_COUNT(ov[1]);
  result = Tcl_EvalObjv(interp, 2, ov, 0);
  DECR_REF_COUNT(ov[1]);

  return result;
}

#if defined(NSF_EXPERIMENTAL)
static int NsfCallArgumentUnknownHandler(Tcl_Interp *interp,
                              Tcl_Obj *methodObj,
                              Tcl_Obj *argumentObj,
                              NsfObject *object) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
NsfCallArgumentUnknownHandler(Tcl_Interp *interp,
                              Tcl_Obj *methodObj,
                              Tcl_Obj *argumentObj,
                              NsfObject *object) {

  Tcl_Obj *ov[4];
  int result, oc = 3;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodObj != NULL);
  nonnull_assert(argumentObj != NULL);
  nonnull_assert(object != NULL);

  /*fprintf(stderr, "try ::nsf::argument::unknown for '%s'\n", ObjStr(nameObj));*/

  ov[0] = NsfGlobalObjs[NSF_ARGUMENT_UNKNOWN_HANDLER];
  ov[1] = methodObj;
  ov[2] = argumentObj;
  if (object != NULL) {
    ov[3] = object->cmdName;
    oc ++;
  }

  INCR_REF_COUNT(ov[1]);
  result = Tcl_EvalObjv(interp, oc, ov, 0);
  DECR_REF_COUNT(ov[1]);

  return result;
}
#endif

/*
 *----------------------------------------------------------------------
 * GetClassFromObj --
 *
 *    Lookup an Next Scripting class from the given objPtr. If the class could
 *    not be directly converted and withUnknown is true, the function calls
 *    the unknown function (::nsf::object::unknown) to fetch the class on
 *    demand and retries the conversion.  On success the NsfClass is returned
 *    in the third argument. The objPtr might be converted by this function.
 *
 * Results:
 *    True or false,
 *
 * Side effects:
 *    object type of objPtr might be changed
 *
 *----------------------------------------------------------------------
 */

static int
GetClassFromObj(Tcl_Interp *interp, register Tcl_Obj *objPtr,
                NsfClass **clPtr, int withUnknown) {
  NsfObject *object;
  NsfClass *cls = NULL;
  int result = TCL_OK;
  const char *objName = ObjStr(objPtr);
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(clPtr != NULL);

  cmd = Tcl_GetCommandFromObj(interp, objPtr);
  /*fprintf(stderr, "GetClassFromObj %p %s unknown %d cmd %p\n", objPtr, objName, withUnknown, cmd);*/

  if (cmd != NULL) {
    cls = NsfGetClassFromCmdPtr(cmd);
#if 1
    if (cls == NULL) {
      /*
       * We have a cmd, but no class; namespace-imported classes are already
       * resolved, but we have to care, if a class is "imported" via "interp
       * alias".
       */
      Tcl_Interp *alias_interp;
      const char *alias_cmd_name;
      Tcl_Obj *nameObj = objPtr;
      Tcl_Obj **alias_ov;
      int alias_oc = 0;

      if (!isAbsolutePath(objName)) {
        nameObj = NameInNamespaceObj(objName, CallingNameSpace(interp));
        objName = ObjStr(nameObj);
        INCR_REF_COUNT(nameObj);
      }

      result = Tcl_GetAliasObj(interp, objName,
                               &alias_interp, &alias_cmd_name, &alias_oc, &alias_ov);
      Tcl_ResetResult(interp);

      /* we only want interp-aliases with 0 args */
      if (likely(result == TCL_OK) && alias_oc == 0) {
        cmd = NSFindCommand(interp, alias_cmd_name);
        /*fprintf(stderr, "..... alias arg 0 '%s' cmd %p\n", alias_cmd_name, cmd);*/
        if (cmd != NULL) {
          cls = NsfGetClassFromCmdPtr(cmd);
        }
      }

      /*fprintf(stderr, "..... final cmd %p, cls %p\n", cmd , cls);*/
      if (nameObj != objPtr) {
        DECR_REF_COUNT(nameObj);
      }
    }
#endif
    if (likely(cls != NULL)) {
      *clPtr = cls;
      return TCL_OK;
    }
  }

  result = GetObjectFromObj(interp, objPtr, &object);
  if (likely(result == TCL_OK)) {
    cls = NsfObjectToClass(object);
    if (likely(cls != NULL)) {
      *clPtr = cls;
      return TCL_OK;
    } else {
      /* flag, that we could not convert so far */
      result = TCL_ERROR;
    }
  }

  if (withUnknown != 0) {
    result = NsfCallObjectUnknownHandler(interp, isAbsolutePath(objName) ? objPtr :
                                         NameInNamespaceObj(objName, CallingNameSpace(interp)));

    if (likely(result == TCL_OK)) {
      /* Retry, but now, the last argument (withUnknown) has to be 0 */
      result = GetClassFromObj(interp, objPtr, clPtr, 0);
    }
    /*fprintf(stderr, "... ::nsf::object::unknown for '%s',
      result %d cl %p\n", objName, result, cl);*/
  }

  return result;
}

/*
 * Version of GetClassFromObj() with external symbol
 */
int
NsfGetClassFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
                   NsfClass **clPtr, int withUnknown) {

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(clPtr != NULL);

  return GetClassFromObj(interp, objPtr, clPtr, withUnknown);
}
/*
 *----------------------------------------------------------------------
 * IsObjectOfType --
 *
 *    Check, if the provided NsfObject is of a certain type. The arguments
 *    "what" and "objPtr" are just used for the error messages. objPtr is the
 *    value from which the object was converted from.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int IsObjectOfType(Tcl_Interp *interp, NsfObject *object, const char *what, Tcl_Obj *objPtr,
               Nsf_Param const *pPtr) nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

static int
IsObjectOfType(Tcl_Interp *interp, NsfObject *object, const char *what, Tcl_Obj *objPtr,
               Nsf_Param const *pPtr) {
  NsfClass *cl;
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(what != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);

  if (unlikely((pPtr->flags & NSF_ARG_BASECLASS) != 0u) && !IsBaseClass(object)) {
    what = "baseclass";
    goto type_error;
  }
  if (unlikely((pPtr->flags & NSF_ARG_METACLASS) != 0u) && !IsMetaClass(interp, (NsfClass *)object, 1)) {
    what = "metaclass";
    goto type_error;
  }

  if (likely(pPtr->converterArg == NULL)) {
    return TCL_OK;
  }
  if (likely((GetClassFromObj(interp, pPtr->converterArg, &cl, 0) == TCL_OK))
      && IsSubType(object->cl, cl)) {
    return TCL_OK;
  }

 type_error:
  DSTRING_INIT(dsPtr);
  Tcl_DStringAppend(dsPtr, what, -1);
  if (pPtr->converterArg != NULL) {
    Tcl_DStringAppend(dsPtr, " of type ", -1);
    Tcl_DStringAppend(dsPtr, ObjStr(pPtr->converterArg), -1);
  }
  NsfObjErrType(interp, NULL, objPtr, Tcl_DStringValue(dsPtr), (Nsf_Param *)pPtr);
  DSTRING_FREE(dsPtr);

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 * NameInNamespaceObj --
 *
 *    Create a fully qualified name in the provided namespace or in
 *    the current namespace in form of an Tcl_Obj (with 0 refCount);
 *
 * Results:
 *    Tcl_Obj containing fully qualified name
 *
 * Side effects:
 *    Allocates fresh copies of list elements
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *
NameInNamespaceObj(const char *name, Tcl_Namespace *nsPtr) {
  Tcl_Obj *objPtr;
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(name != NULL);
  nonnull_assert(nsPtr != NULL);

  /*fprintf(stderr, "NameInNamespaceObj %s (%p, %s) ", name, nsPtr, nsPtr->fullName);*/

  DSTRING_INIT(dsPtr);
  DStringAppendQualName(dsPtr, nsPtr, name);
  objPtr = Tcl_NewStringObj(Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));

  /*fprintf(stderr, "returns %s\n", ObjStr(objPtr));*/
  DSTRING_FREE(dsPtr);
  return objPtr;
}

/*
 *----------------------------------------------------------------------
 * NewTclCommand --
 *
 *    Given a provided prefix in dsPtr, make it a name of a command that does not exist.
 *    This function is used by the *new command, when "anonymous" objects are created
 *
 * Results:
 *    dsPtr will be complete to represent a new (unused) name of a command
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
void NewTclCommand(Tcl_Interp *interp, Tcl_DString *dsPtr) nonnull(1) nonnull(2);

void
NewTclCommand(Tcl_Interp *interp, Tcl_DString *dsPtr) {
  int prefixLength;
  NsfStringIncrStruct *iss;

  nonnull_assert(interp != NULL);
  nonnull_assert(dsPtr != NULL);

  prefixLength = dsPtr->length;
  iss = &RUNTIME_STATE(interp)->iss;

  while (1) {

    (void)NsfStringIncr(iss);
    Tcl_DStringAppend(dsPtr, iss->start, iss->length);
    if (!Tcl_FindCommand(interp, Tcl_DStringValue(dsPtr), NULL, TCL_GLOBAL_ONLY)) {
      break;
    }
    /*
     * In case the symbol existed already, reset prefix to the
     * original length.
     */
    Tcl_DStringSetLength(dsPtr, prefixLength);
  }
}

/*
 *----------------------------------------------------------------------
 * NsfReverseClasses --
 *
 *    Reverse class list. Caller is responsible for freeing data.
 *
 * Results:
 *    Pointer to start of the reversed list
 *
 * Side effects:
 *    Allocates fresh copies of list elements
 *
 *----------------------------------------------------------------------
 */
static NsfClasses *NsfReverseClasses(NsfClasses *sl) nonnull(1) returns_nonnull;

static NsfClasses *
NsfReverseClasses(NsfClasses *sl) {
  NsfClasses *firstPtr = NULL;

  nonnull_assert(sl != NULL);

  do {
    NsfClasses *element = NEW(NsfClasses);

    element->cl = sl->cl;
    element->clientData = sl->clientData;
    element->nextPtr = firstPtr;
    firstPtr = element;

    sl = sl->nextPtr;
  } while (likely(sl != NULL));

  return firstPtr;
}

/*
 *----------------------------------------------------------------------
 * NsfClassListFree --
 *
 *    Frees all elements of the provided class list
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees memory.
 *
 *----------------------------------------------------------------------
 */
static void NsfClassListFree(NsfClasses *classList) nonnull(1);

static void
NsfClassListFree(NsfClasses *classList) {
  NsfClasses *nextPtr;

 nonnull_assert(classList != NULL);

 do {
   nextPtr = classList->nextPtr;
   FREE(NsfClasses, classList);
   classList = nextPtr;
 } while (likely(classList != NULL));
}

/*
 *----------------------------------------------------------------------
 * NsfClassListAdd --
 *
 *    Add class list entry to the specified list. In case the initial
 *    list is empty, *firstPtrPtr is updated as well.
 *
 * Results:
 *    Returns address of next-pointer.
 *
 * Side effects:
 *    New list element is allocated.
 *
 *----------------------------------------------------------------------
 */

static NsfClasses **
NsfClassListAdd(NsfClasses **firstPtrPtr, NsfClass *cl, ClientData clientData) {
  NsfClasses *classListPtr, *element = NEW(NsfClasses);

  nonnull_assert(firstPtrPtr != NULL);

  element->cl = cl;
  element->clientData = clientData;
  element->nextPtr = NULL;

  classListPtr = *firstPtrPtr;
  if (classListPtr != NULL) {
    while (classListPtr->nextPtr) {
      classListPtr = classListPtr->nextPtr;
    }
    classListPtr->nextPtr = element;
  } else {
    *firstPtrPtr = element;
  }
  return &(element->nextPtr);
}

/*
 *----------------------------------------------------------------------
 * NsfClassListAddNoDup --
 *
 *    Add class list entry to the specified list without duplicates. In case
 *    the initial list is empty, *firstPtrPtr is updated as well.
 *
 * Results:
 *    Returns address of next pointer.
 *
 * Side effects:
 *    New list element is allocated.
 *
 *----------------------------------------------------------------------
 */

static NsfClasses **NsfClassListAddNoDup(NsfClasses **firstPtrPtr, NsfClass *cl,
                                         ClientData clientData, int *isNewPtr)
  nonnull(1) nonnull(2);

static NsfClasses **
NsfClassListAddNoDup(NsfClasses **firstPtrPtr, NsfClass *cl, ClientData clientData, int *isNewPtr) {
  NsfClasses *clPtr, **nextPtr;

  nonnull_assert(firstPtrPtr != NULL);
  nonnull_assert(cl != NULL);

  clPtr = *firstPtrPtr;
  if (clPtr != NULL) {
    for (; (clPtr->nextPtr != NULL) && (clPtr->cl != cl); clPtr = clPtr->nextPtr);
    nextPtr = &clPtr->nextPtr;
  } else {
    nextPtr = firstPtrPtr;
  }

  if (*nextPtr == NULL) {
    NsfClasses *element = NEW(NsfClasses);

    element->cl = cl;
    element->clientData = clientData;
    element->nextPtr = NULL;
    *nextPtr = element;
    if (isNewPtr != NULL) {
      *isNewPtr = 1;
    }
  } else {
    if (isNewPtr != NULL) {
      *isNewPtr = 0;
    }
  }
  return nextPtr;
}

/*
 *----------------------------------------------------------------------
 * NsfClassListFind --
 *
 *    Find an element in the class list and return it if found.
 *
 * Results:
 *    Found element or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfClasses *NsfClassListFind(NsfClasses *clPtr, NsfClass *cl) nonnull(2);

static NsfClasses *
NsfClassListFind(NsfClasses *clPtr, NsfClass *cl) {

  nonnull_assert(cl != NULL);

  for (; clPtr; clPtr = clPtr->nextPtr) {
    if (clPtr->cl == cl) {
      break;
    }
  }
  return clPtr;
}

#if defined(NSF_CLASSLIST_PRINT)
/* debugging purposes only */
/*
 *----------------------------------------------------------------------
 * NsfClassListStats --
 *
 *    Print some statistics about generated Class List structures for
 *    debugging purpose.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static void NsfClassListStats(const char *title, NsfClasses *classList) nonnull(1);

static void
NsfClassListStats(const char *title, NsfClasses *classListPtr) {
  NsfClass *cl;
  int count = 0;

  nonnull_assert(title != NULL);

  cl = (classListPtr != NULL) ? classListPtr->cl : NULL;
  for (; classListPtr; classListPtr = classListPtr->nextPtr) {
    count++;
  }

  fprintf(stderr, "%s class list starting with %s has %d elements\n",
          title, (cl != NULL) ? ClassName(cl) : "none", count);
}

static void NsfClassListPrint(const char *title, NsfClasses *clsList) nonnull(1);

static void
NsfClassListPrint(const char *title, NsfClasses *clsList) {

  nonnull_assert(title != NULL);

  fprintf(stderr, "%s", title);
  /* fprintf(stderr, " %p:", clsList); */
  while (clsList) {
    /* fprintf(stderr, " %p", clsList->cl); */
    fprintf(stderr, " %p", clsList);
    fprintf(stderr, " %s", ClassName(clsList->cl));
    clsList = clsList->nextPtr;
  }
  fprintf(stderr, "\n");
}
#endif

/*
 *----------------------------------------------------------------------
 * NsfClassListUnlink --
 *
 *    Return removed item with matching key form nsfClasses.
 *    Key is void to allow not only class pointers as keys.
 *
 * Results:
 *    unlinked element or NULL.
 *    In case the first element is unlinked, *firstPtrPtr
 *    is updated.
 *
 * Side effects:
 *    none.
 *
 *----------------------------------------------------------------------
 */
static NsfClasses *NsfClassListUnlink(NsfClasses **firstPtrPtr, void *key) nonnull(1) nonnull(2);

static NsfClasses *
NsfClassListUnlink(NsfClasses **firstPtrPtr, void *key) {
  NsfClasses *entryPtr = NULL;

  nonnull_assert(firstPtrPtr != NULL);
  nonnull_assert(key != NULL);

  if (*firstPtrPtr != NULL) {
    NsfClasses *prevPtr = NULL;

    /* list is non-empty */
    for (entryPtr = *firstPtrPtr; entryPtr; prevPtr = entryPtr, entryPtr = entryPtr->nextPtr) {
      if ((void *)entryPtr->cl == key) {
        /* found entry */
        if (prevPtr != NULL) {
          /* later item */
          prevPtr->nextPtr = entryPtr->nextPtr;
        } else {
          /* first item */
          *firstPtrPtr = entryPtr->nextPtr;
        }
        entryPtr->nextPtr = NULL;
        break;
      }
    }
  }

  return entryPtr;
}


/*
 * Functions for computing Precedence Order
 */

/*
 *----------------------------------------------------------------------
 * TopoSort --
 *
 *    Perform a topological sort of the class hierarchies. Depending on the
 *    argument "direction" it performs the sort on the transitive list of
 *    superclasses or subclasses. The resulting list contains no duplicates or
 *    cycles and is returned in the class member "order". During the
 *    computation it colors the processed nodes in WHITE, GRAY or BLACK.
 *
 * Results:
 *    Boolean value indicating success.
 *
 * Side effects:
 *    Allocating class list.
 *
 *----------------------------------------------------------------------
 */

enum colors { WHITE, GRAY, BLACK };
typedef enum { SUPER_CLASSES, SUB_CLASSES } ClassDirection;

static int TopoSort(NsfClass *cl, NsfClass *baseClass, ClassDirection direction, int withMixinOfs)
  nonnull(1) nonnull(2);

static int
TopoSort(NsfClass *cl, NsfClass *baseClass, ClassDirection direction, int withMixinOfs) {
  NsfClasses *sl, *pl;

  nonnull_assert(cl != NULL);
  nonnull_assert(baseClass != NULL);

  sl = direction == SUPER_CLASSES ? cl->super : cl->sub;

  /*
   * Be careful to reset the color of unreported classes to
   * white in case we unwind with error, and on final exit
   * reset color of reported classes to WHITE. Meaning of colors:
   *
   *     WHITE ... not processed
   *     GRAY  ... in work
   *     BLACK ... done
   */

  cl->color = GRAY;
  for (; sl; sl = sl->nextPtr) {
    NsfClass *sc = sl->cl;
    if (sc->color == GRAY) { cl->color = WHITE; return 0; }
    if (unlikely(sc->color == WHITE && !TopoSort(sc, baseClass, direction, withMixinOfs))) {
      cl->color = WHITE;
      if (cl == baseClass) {
        register NsfClasses *pc;
        for (pc = cl->order; pc; pc = pc->nextPtr) { pc->cl->color = WHITE; }
      }
      return 0;
    }
  }
  if (withMixinOfs != 0) {
    NsfCmdList *classMixins = ((cl->opt != NULL) && cl->opt->isClassMixinOf) ? cl->opt->isClassMixinOf : NULL;

    for (; classMixins; classMixins = classMixins->nextPtr) {
      NsfClass *sc = NsfGetClassFromCmdPtr(classMixins->cmdPtr);

      if (unlikely(sc->color == WHITE && !TopoSort(sc, baseClass, direction, withMixinOfs))) {
        NsfLog(sc->object.teardown, NSF_LOG_WARN,
               "cycle in the mixin graph list detected for class %s",
               ClassName(sc));
      }
    }
  }
  cl->color = BLACK;
  pl = NEW(NsfClasses);
  pl->cl = cl;
  pl->nextPtr = baseClass->order;
  baseClass->order = pl;
  if (unlikely(cl == baseClass)) {
    register NsfClasses *pc;
    for (pc = cl->order; pc; pc = pc->nextPtr) { pc->cl->color = WHITE; }
  }
  return 1;
}


/*
 *----------------------------------------------------------------------
 * MustBeBefore --
 *
 *    Check the partial ordering of classes based on precedence list in the
 *    form of prior ordering from the topological sort. We compare here
 *    orderings based the class hierarchies with single inheritance and prior
 *    solved multiple inheritance orderings. The test is true, if b must be
 *    before a.
 *
 * Results:
 *    Boolean value indicating success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int MustBeBefore(NsfClass *a, NsfClass *b, NsfClasses *superClasses) nonnull(1) nonnull(2) nonnull(3);

static int
MustBeBefore(NsfClass *a, NsfClass *b, NsfClasses *superClasses) {
  int result;

  nonnull_assert(a != NULL);
  nonnull_assert(b != NULL);
  nonnull_assert(superClasses != NULL);
  assert(b->order != NULL);

  /*
   * Check, if a is in the precedence order of b. E.g.
   *
   *   a c1 object
   *   b c2 a object
   *
   * If so then b must be before a to preserve the precedence order based on
   * single inheritance (monotonicity).
   */
  result = (NsfClassListFind(b->order, a) != NULL);

  /*
   * When the partital ordering can't be decided based on the local order
   * test, we take the specified multiple inheritance ordering in superClasses
   * (e.g. coming from -superclass {x y}) which is not taken account by the
   * class hierarchy.
   */
  if (result == 0) {
    NsfClasses *sl;
    int bFound = 0;

#if defined(NSF_LINEARIZER_TRACE)
    fprintf(stderr, "--> check %s before %s?\n", ClassName(b), ClassName(a));
    NsfClassListPrint("miList", miList);
#endif
    for (sl = superClasses; sl; sl = sl->nextPtr) {
      if (sl->cl == b) {
        bFound = 1;
      } else if (bFound && sl->cl == a) {
#if defined(NSF_LINEARIZER_TRACE)
        fprintf(stderr, "%s in inheritanceList before %s therefore a < b\n", ClassName(b), ClassName(a));
#endif
        result = 1;
        break;
      }
    }
  }

#if defined(NSF_LINEARIZER_TRACE)
  fprintf(stderr, "compare a: %s %p b: %s %p -> %d\n", ClassName(a), a->order, ClassName(b), b->order, result);
  NsfClassListPrint("\ta", a->order);
  NsfClassListPrint("\tb", b->order);
#endif
  return result;
}


/*
 *----------------------------------------------------------------------
 * ValidClassListTail --
 *
 *    Debug function to assure that the provided class lists are valid. The
 *    tail of the class list must be a base class of the current object
 *    system.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
#if !defined(NDEBUG)
static void ValidClassListTail(const char *what, NsfClasses *classListPtr) {
  NsfClasses *sl, *tail;

  for (sl = classListPtr, tail = NULL; sl; sl = sl->nextPtr) {tail = sl;}
  if (tail != NULL) {
    /* fprintf(stderr, "check tail what %s %p\n", what, ClassName(tail->cl), tail->nextPtr);*/
    assert(IsBaseClass(&tail->cl->object));
    assert(tail->nextPtr == NULL);
  }
}
#else
# define ValidClassListTail(what, classListPtr)
#endif

/*
 *----------------------------------------------------------------------
 * MergeInheritanceLists --
 *
 *    Merge the PrecedenceOrders of class cl. This function is called, when cl
 *    is defined with multiple inheritance. The precedence orders of the
 *    specified classes are merged in an order preserving manner to achieve
 *    monotonicity.
 *
 * Results:
 *    precedence order.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfClasses *MergeInheritanceLists(NsfClasses *pl, NsfClass *cl) nonnull(1) nonnull(2) returns_nonnull;

static NsfClasses *
MergeInheritanceLists(NsfClasses *pl, NsfClass *cl) {

  NsfClasses *sl, *baseList, **plNext, *superClasses,
    *deletionList = NULL;

  nonnull_assert(pl != NULL);
  nonnull_assert(cl != NULL);

#if defined(NSF_LINEARIZER_TRACE)
  fprintf(stderr, "=== working on %s\n", ClassName(cl));
#endif

  /*
   * The available multiple inheritance list is in reversed order so we have
   * to reverse it to obtain the specified superClasses in the provided order.
   */
  superClasses = NsfReverseClasses(cl->super);

  /*
   * We distinguish between a
   *
   *  - baseList (which might be later an result of partial merges), and a
   *  - mergeList, which is merged order-preserving into the baseList.
   *
   * The first baseList is the precedence list of the first element of the
   * specified superClasses.
   */

  baseList = superClasses->cl->order;
  assert(baseList != NULL);

#if defined(NSF_LINEARIZER_TRACE)
  fprintf(stderr, "=== baseList from %s\n", ClassName(superClasses->cl));
  NsfClassListPrint("baseList", baseList);
#endif

  /*
   * The first element of the result list of the merge operation is the first
   * element of the baseList.
   */
  plNext = NsfClassListAdd(&pl, baseList->cl, NULL);

  /*
   * For every element but the first (which is already in baseList), we have to
   * perform the merge operation. For n elements in superClasses, the merge
   * operation is performed n-1 times.
   */
  for (sl = superClasses->nextPtr; sl; sl = sl->nextPtr) {
    NsfClasses *mergeList = sl->cl->order, *baseListCurrent;

#if defined(NSF_LINEARIZER_TRACE)
    NsfClassListPrint("mergeList", mergeList);
#endif

    /*
     * Merge mergeList into baseList. We start with the 2nd (later probably
     * nth) entry of the baseList
     */
    baseListCurrent = baseList->nextPtr;
    assert(baseListCurrent != NULL);

    while (mergeList != NULL) {
      NsfClass *addClass;

      ValidClassListTail("baseList",  baseList);
      ValidClassListTail("mergeList", mergeList);

      assert(baseListCurrent != NULL);
      /* NsfClassListPrint("baseListCurrent", baseListCurrent); */

      if (mergeList->cl == baseListCurrent->cl) {
        /*
         * The first element of mergeList and the current baseList element are
         * identical. The element is in the result, keep the element in the
         * result, advance in both lists.
         */
        /* fprintf(stderr, "\t\tadvance both\n"); */
        addClass = mergeList->cl;
        baseListCurrent = baseListCurrent->nextPtr;
        mergeList = mergeList->nextPtr;

      } else if (MustBeBefore(baseListCurrent->cl, mergeList->cl, superClasses)) {
        /*
         * Check, if current element of mergeList must be before the current
         * element of baseList. If so, insert current mergelist element before
         * baseListCurrent,
         */
        addClass  = mergeList->cl;
        mergeList = mergeList->nextPtr;
        /* fprintf(stderr, "\t\tadd from mergeList %s\n", ClassName(addClass)); */

      } else {
        /*
         * Two cases above do not apply, add from baseList and advance
         * baseList pointer.
         */
        addClass = baseListCurrent->cl;
        baseListCurrent = baseListCurrent->nextPtr;
        /* fprintf(stderr, "\t\tadd from baselist %s\n", ClassName(addClass)); */
      }

      if (addClass != NULL) {
        /*
         * We have to add an element to the precedence list. When the class to
         * be added is already in the result list (which might happen just in
         * crippled cases) then delete it. In the final step it will be added
         * again to the end.
         */
        NsfClasses *deletedElement = NsfClassListUnlink(&pl, addClass);

        if (deletedElement != NULL) {
#if defined(NSF_LINEARIZER_TRACE)
          fprintf(stderr, "\t\t%s is redundant (in resultList)\n", ClassName(addClass));
#endif
          /*
           * When plNext points to the nextPtr of the deleted element, search
           * the list from the begin
           */
          if (plNext == &(deletedElement->nextPtr)) {
            plNext = &pl;
          }
          NsfClassListFree(deletedElement);
        }
        /* add the new element */
        plNext = NsfClassListAdd(plNext, addClass, NULL);
      }

#if defined(NSF_LINEARIZER_TRACE)
      NsfClassListPrint("pl:", pl);
#endif
    }
    /*
     * mergeList is processed, we have a final precedence list in pl.  In case
     * are at then of superClasses, we are done. Otherwise, use the resulting
     * pl as next baseList and continue with the next mergeList from
     * superClasses.
     */
#if defined(NSF_LINEARIZER_TRACE)
    NsfClassListPrint("plFinal:", pl);
#endif

    if (sl->nextPtr != NULL) {
      /* We are not at the end, use pl as new base list */
      baseList = pl;

#if defined(NSF_LINEARIZER_TRACE)
      fprintf(stderr, "=== setting new baseList\n");
      NsfClassListPrint("new baseList", baseList);
#endif
      /*
       * Add old pl to deletion list; these entries are deleted once merging
       * is finished.
       */
      NsfClassListAdd(&deletionList, NULL, pl);

      /*
       * Create a fresh pl for the next iteration.
       */
      pl = NULL;
      plNext = NsfClassListAdd(&pl, cl, NULL);
    }
  }

  for (sl = deletionList; sl; sl = sl->nextPtr) {
    /* fprintf(stderr, "delete from deletion list %p client data %p\n", sl, sl->clientData); */
    NsfClassListFree(sl->clientData);
  }

  if (deletionList != NULL) {
    NsfClassListFree(deletionList);
  }
  NsfClassListFree(superClasses);

  return pl;
}

#if defined(NDEBUG)
#define AssertOrderIsWhite(arg)
#else
static void AssertOrderIsWhite(NsfClasses *order) {
  register NsfClasses *pc;
  for (pc = order; pc; pc = pc->nextPtr) { assert(pc->cl->color == WHITE); }
}
#endif

/*
 *----------------------------------------------------------------------
 * TopoSortSuper --
 *
 *    Compute the precedence order for baseClass based on the superclasses. If
 *    the order is computable, update base class and return 1. Otherwise
 *    return 0.
 *
 * Results:
 *    Success/Failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int TopoSortSuper(NsfClass *cl, NsfClass *baseClass) nonnull(1) nonnull(2);

static int
TopoSortSuper(NsfClass *cl, NsfClass *baseClass) {
  NsfClasses *pl, *sl;

  nonnull_assert(cl != NULL);
  nonnull_assert(baseClass != NULL);

  /*
   * Be careful to reset the color of unreported classes to
   * white in the caller on all exits to WHITE.
   *
   *     WHITE ... not processed
   *     GRAY  ... in work
   *     BLACK ... done
   */

  cl->color = GRAY;
  for (sl = cl->super; likely(sl != NULL); sl = sl->nextPtr) {
    NsfClass *sc = sl->cl;

    if (sc->color == GRAY) { cl->color = WHITE; return 0; }
    if (unlikely(sc->color == WHITE && !TopoSortSuper(sc, baseClass))) {
      cl->color = WHITE;
      return 0;
    }
  }

  /*
   * Create a new precedence list containing cl.
   */
  pl = NEW(NsfClasses);
  pl->cl = cl;
  pl->nextPtr = NULL;

  /*
   * If we have multiple inheritance we merge the precomputed inheritance
   * orders of the involved classes in the provided order.
   */
  if (likely(cl->super != NULL) && unlikely(cl->super->nextPtr != NULL)) {

    pl = MergeInheritanceLists(pl, cl);

    if (baseClass->order != NULL) {
      NsfClassListFree(baseClass->order);
      baseClass->order = NULL;
    }


  } else {
    /*
     * Add baseClass order to the end of the precedence list.
     */
    assert(pl->nextPtr == NULL);
    pl->nextPtr = baseClass->order;
  }

  cl->color = BLACK;
  /*
   * Set baseclass order to the newly computed list (the result of this function)
   */
  baseClass->order = pl;

  return 1;
}


/*
 *----------------------------------------------------------------------
 * PrecedenceOrder --
 *
 *    Return a class list containing the transitive list of super classes
 *    starting with (and containing) the provided class. The super class list
 *    is cached in cl->order and has to be invalidated by FlushPrecedences()
 *    in case the order changes. The caller does not have to free the returned
 *    class list (like for TransitiveSubClasses);
 *
 * Results:
 *    Class list, NULL on error
 *
 * Side effects:
 *    Updating cl->order.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static NsfClasses *PrecedenceOrder(NsfClass *cl) nonnull(1);

NSF_INLINE static NsfClasses *
PrecedenceOrder(NsfClass *cl) {
  register NsfClasses *sl;
  int success;

  nonnull_assert(cl != NULL);

  /*
   * Check, of the superclass order is already cached.
   */
  if (likely(cl->order != NULL)) {
    return cl->order;
  }

  /*
   * For multiple inheritance (more than one superclass), make sure that
   * required precedence orders are precomputed.
   */

  if (likely(cl->super != NULL) && unlikely(cl->super->nextPtr != NULL)) {
    for (sl = cl->super; sl; sl = sl->nextPtr) {
      NsfClasses *pl;

#if defined(NSF_LINEARIZER_TRACE)
      fprintf(stderr, "====== PrecedenceOrder mi, check %s %p \n",
              ClassName(sl->cl), sl->cl->order);
#endif
      if (unlikely(sl->cl->order == NULL) && likely(cl != sl->cl)) {

#if defined(NSF_LINEARIZER_TRACE)
        fprintf(stderr, "====== PrecedenceOrder computes required order for %s \n",
                ClassName(sl->cl));
#endif
        PrecedenceOrder(sl->cl);
#if defined(NSF_LINEARIZER_TRACE)
        NsfClassListPrint("====== PO:", sl->cl->order);
#endif
      }

      for (pl = sl->cl->order; pl; pl = pl->nextPtr) {
#if defined(NSF_LINEARIZER_TRACE)
        fprintf(stderr, "====== PO order: %s %p\n", ClassName(pl->cl), pl->cl->order);
#endif
        if (pl->cl->order == NULL) {
#if defined(NSF_LINEARIZER_TRACE)
          fprintf(stderr, "========== recurse\n");
#endif
          PrecedenceOrder(pl->cl);
        }
      }
    }
  }

  success = TopoSortSuper(cl, cl);

  /*
   * Reset the color of all nodes.
   */
  for (sl = cl->order; sl; sl = sl->nextPtr) {
    sl->cl->color = WHITE;
  }

  /*
   * If computation is successful, return cl->order.
   * Otherwise clear cl->order.
   */
  if (likely(success)) {
    AssertOrderIsWhite(cl->order);
    return cl->order;
  } else {
    NsfClassListFree(cl->order);
    return cl->order = NULL;
  }
}

/*
 *----------------------------------------------------------------------
 * TransitiveSubClasses --
 *
 *    Return a class list containing the transitive list of sub-classes
 *    starting with (and containing) the provided class.The caller has to free
 *    the returned class list.
 *
 * Results:
 *    Class list or NULL if cycles are detected
 *
 * Side effects:
 *    Just indirect.
 *
 *----------------------------------------------------------------------
 */

NSF_INLINE static NsfClasses * TransitiveSubClasses(NsfClass *cl) nonnull(1);

NSF_INLINE static NsfClasses *
TransitiveSubClasses(NsfClass *cl) {
  NsfClasses *order, *savedOrder;

  nonnull_assert(cl != NULL);

  /*
   * Since TopoSort() places its result in cl->order, we have to save the old
   * cl->order, perform the computation and restore the old order.
   */
  savedOrder = cl->order;
  cl->order = NULL;

  if (likely(TopoSort(cl, cl, SUB_CLASSES, 0))) {
    order = cl->order;
  } else {
    if (cl->order != NULL) {
      NsfClassListFree(cl->order);
    }
    order = NULL;
  }

  /*
   * TODO: if this holds, we can change the fn to returns_nonnull and the else-branch is not needed
   */
  assert(order != NULL);
  AssertOrderIsWhite(order);

  cl->order = savedOrder;
  return order;
}

/*
 *----------------------------------------------------------------------
 * DependentSubClasses --
 *
 *    Return a class list containing all dependent classes (i.e. classes that
 *    inherit via intrinsic or mixin hierarchy) starting with (and containing)
 *    the provided class.The caller has to free the returned class list.
 *
 * Results:
 *    Class list or NULL if cycles are detected
 *
 * Side effects:
 *    Just indirect.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static NsfClasses *DependentSubClasses(NsfClass *cl) nonnull(1);

NSF_INLINE static NsfClasses *
DependentSubClasses(NsfClass *cl) {
  NsfClasses *order, *savedOrder;

  nonnull_assert(cl != NULL);

  /*
   * Since TopoSort() places its result in cl->order, we have to save the old
   * cl->order, perform the computation and restore the old order.
   */
  savedOrder = cl->order;
  cl->order = NULL;

  if (likely(TopoSort(cl, cl, SUB_CLASSES, 1))) {
    order = cl->order;
  } else {
    if (cl->order != NULL) {
      NsfClassListFree(cl->order);
    }
    order = NULL;
  }

  /*
   * TODO: if this holds, we can change the fn to returns_nonnull and the else-branch is not needed
   */

  assert(order != NULL);
  AssertOrderIsWhite(order);

  cl->order = savedOrder;
  return order;
}


/*
 *----------------------------------------------------------------------
 * FlushPrecedences --
 *
 *    This function iterations over the provided class list and flushes (and
 *    frees) the superclass caches in cl->order for every element.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Freeing class lists cached in cl->order.
 *
 *----------------------------------------------------------------------
 */
static void FlushPrecedences(NsfClasses *subClasses) nonnull(1);

static void
FlushPrecedences(NsfClasses *clPtr) {

  nonnull_assert(clPtr != NULL);

  do {
    if (clPtr->cl->order != NULL) {
      NsfClassListFree(clPtr->cl->order);
    }
    clPtr->cl->order = NULL;
    clPtr = clPtr->nextPtr;
  } while (clPtr != NULL);
}


/*
 *----------------------------------------------------------------------
 * AddInstance --
 *
 *    Add an instance to a class.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Add entry to children hash-table.
 *
 *----------------------------------------------------------------------
 */
static void AddInstance(NsfObject *object, NsfClass *cl) nonnull(1) nonnull(2);

static void
AddInstance(NsfObject *object, NsfClass *cl) {
  int isNewItem;

  nonnull_assert(object != NULL);
  nonnull_assert(cl != NULL);

  object->cl = cl;
  (void) Tcl_CreateHashEntry(&cl->instances, (char *)object, &isNewItem);
  /*if (newItem == 0) {
    fprintf(stderr, "instance %p %s was already an instance of %p %s\n", object, ObjectName(object), cl, ClassName(cl));
    }*/
  assert(isNewItem != 0);
}


/*
 *----------------------------------------------------------------------
 * RemoveInstance --
 *
 *    Remove an instance from a class. The function checks, whether the entry
 *    is actually still an instance before it deletes it.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Entry deleted from instances hash-table
 *
 *----------------------------------------------------------------------
 */
static void RemoveInstance(NsfObject *object, NsfClass *cl) nonnull(1) nonnull(2);

static void
RemoveInstance(NsfObject *object, NsfClass *cl) {

  nonnull_assert(object != NULL);
  nonnull_assert(cl != NULL);

  /*
   * If we are during a delete, which should not happen under normal
   * operations, prevent an abort due to a deleted hash table.
   */
  if ((cl->object.flags & NSF_DURING_DELETE) != 0u) {
    NsfLog(cl->object.teardown, NSF_LOG_WARN,
           "Class which should loose instance is currently being deleted: %s",
            ObjStr((cl)->object.cmdName));
  } else {
    Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&cl->instances, (char *)object, NULL);

    /*if (hPtr == NULL) {
      fprintf(stderr, "instance %s is not an instance of %s\n", ObjectName(object), ClassName(cl));
      }*/
    assert(hPtr != NULL);
    Tcl_DeleteHashEntry(hPtr);
  }
}

/*
 * superclass/subclass list maintenance
 */
static void AddSuper1(NsfClass *s, NsfClasses **sl) nonnull(1) nonnull(2);
static void AddSuper(NsfClass *cl, NsfClass *super) nonnull(1);
static int RemoveSuper1(NsfClass *s, NsfClasses **sl) nonnull(1) nonnull(2);
static int RemoveSuper(NsfClass *cl, NsfClass *super) nonnull(1) nonnull(2);

static void
AddSuper1(NsfClass *s, NsfClasses **sl) {
  NsfClasses *sc = NEW(NsfClasses);

  nonnull_assert(s != NULL);
  nonnull_assert(sl != NULL);

  sc->cl = s;
  sc->nextPtr = *sl;
  *sl = sc;
}

static void
AddSuper(NsfClass *cl, NsfClass *super) {

  nonnull_assert(cl != NULL);

  if (super != NULL) {
    /*
     * keep corresponding sub in step with super
     */
    AddSuper1(super, &cl->super);
    AddSuper1(cl, &super->sub);
  }
}

static int
RemoveSuper1(NsfClass *s, NsfClasses **sl) {
  NsfClasses *l;

  nonnull_assert(s != NULL);
  nonnull_assert(sl != NULL);

  l = *sl;

  if (l == NULL) {
    return 0;
  }
  if (l->cl == s) {
    *sl = l->nextPtr;
    FREE(NsfClasses, l);
    return 1;
  }
  while ((l->nextPtr != NULL) && (l->nextPtr->cl != s)) {
    l = l->nextPtr;
  }
  if (l->nextPtr != NULL) {
    NsfClasses *n = l->nextPtr->nextPtr;
    FREE(NsfClasses, l->nextPtr);
    l->nextPtr = n;
    return 1;
  }
  return 0;
}

static int
RemoveSuper(NsfClass *cl, NsfClass *super) {
  int sp, sb;

  nonnull_assert(cl != NULL);
  nonnull_assert(super != NULL);

  /*
   * keep corresponding sub in step with super
   */

  sp = RemoveSuper1(super, &cl->super);
  sb = RemoveSuper1(cl, &super->sub);

  return sp && sb;
}

/*
 * methods lookup
 */

/*
 *----------------------------------------------------------------------
 * GetEnsembleObjectFromName --
 *
 *    Get an ensemble object from a method name.  If the method name
 *    is fully qualified, just use a Tcl lookup, otherwise get it from
 *    the provided namespace,
 *
 * Results:
 *    ensemble object or NULL
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static NsfObject * GetEnsembleObjectFromName(Tcl_Interp *interp, Tcl_Namespace *nsPtr, Tcl_Obj *name,
                         Tcl_Command *cmdPtr, int *fromClassNS)
  nonnull(1) nonnull(3) nonnull(4) nonnull(5);

static NsfObject *
GetEnsembleObjectFromName(Tcl_Interp *interp, Tcl_Namespace *nsPtr, Tcl_Obj *name,
                         Tcl_Command *cmdPtr, int *fromClassNS) {
  Tcl_Command cmd;
  const char *nameString = ObjStr(name);

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  nonnull_assert(cmdPtr != NULL);
  nonnull_assert(fromClassNS != NULL);

  if (*nameString == ':') {
    cmd = Tcl_GetCommandFromObj(interp, name);
    *fromClassNS = IsClassNsName(nameString, NULL);
  } else {
    cmd = (nsPtr != NULL) ? FindMethod(nsPtr, nameString) : NULL;
  }

  if (cmd != NULL) {
    *cmdPtr = cmd;
    return NsfGetObjectFromCmdPtr(GetOriginalCommand(cmd));
  }
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * GetRegObject --
 *
 *    Try to get the object, on which the method was registered from a
 *    fully qualified method handle
 *
 * Results:
 *    NsfObject * or NULL on failure
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static NsfObject *GetRegObject(Tcl_Interp *interp, Tcl_Command cmd, const char *methodName,
                               const char **methodName1, int *fromClassNS)
  nonnull(1) nonnull(3) nonnull(5) nonnull(2);

static NsfObject *
GetRegObject(Tcl_Interp *interp, Tcl_Command cmd, const char *methodName,
             const char **methodName1, int *fromClassNS) {
  NsfObject *regObject;
  const char *procName;
  size_t objNameLength;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(methodName != NULL);
  assert(*methodName == ':');
  nonnull_assert(fromClassNS != NULL);
  nonnull_assert(cmd != NULL);

  procName = Tcl_GetCommandName(interp, cmd);
  objNameLength = strlen(methodName) - strlen(procName) - 2;

  if (objNameLength > 0) {
    Tcl_DString ds, *dsPtr = &ds;
    /* obtain parent name */
    Tcl_DStringInit(dsPtr);
    Tcl_DStringAppend(dsPtr, methodName, objNameLength);
    regObject = GetObjectFromNsName(interp, Tcl_DStringValue(dsPtr), fromClassNS);
    if (regObject != NULL && methodName1 != NULL) {
      *methodName1 = procName;
    }
    Tcl_DStringFree(dsPtr);
  } else {
    regObject = NULL;
  }

  /*fprintf(stderr, "GetRegObject cmd %p methodName '%s' => %p\n", cmd, methodName, regObject);*/
  return regObject;
}

/*
 *----------------------------------------------------------------------
 * ResolveMethodName --
 *
 *    Resolve a method name relative to a provided namespace.
 *    The method name can be
 *      a) a fully qualified name
 *      b) a list of method name and subcommands
 *      c) a simple name
 *
 * Results:
 *    Tcl_Command or NULL on failure
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static Tcl_Command ResolveMethodName(Tcl_Interp *interp, Tcl_Namespace *nsPtr, Tcl_Obj *methodObj,
                  Tcl_DString *methodNameDs,
                  NsfObject **regObject,
                  NsfObject **defObject,
                  const char **methodName1, int *fromClassNS) nonnull(1) nonnull(3) nonnull(8);

static Tcl_Command
ResolveMethodName(Tcl_Interp *interp, Tcl_Namespace *nsPtr, Tcl_Obj *methodObj,
                  Tcl_DString *methodNameDs,
                  NsfObject **regObject,
                  NsfObject **defObject,
                  const char **methodName1, int *fromClassNS) {
  const char *methodName;
  NsfObject *referencedObject;
  int containsSpace, tailContainsSpace;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodObj != NULL);
  nonnull_assert(fromClassNS != NULL);

  methodName = ObjStr(methodObj);

  /*fprintf(stderr,"methodName '%s' comp %d type %s\n",
    methodName, strchr(methodName, ' ')>0, (methodObj->typePtr != NULL) ? methodObj->typePtr->name : "(none)");*/

  if (methodObj->typePtr == Nsf_OT_listType) {
    int length;
    Tcl_ListObjLength(interp, methodObj, &length);
    containsSpace = length > 1;
  } else if (methodObj->typePtr == Nsf_OT_tclCmdNameType) {
    containsSpace = 0;
  } else {
    containsSpace = strchr(methodName, ' ') != NULL;
  }

  if (containsSpace != 0) {
    tailContainsSpace = strchr(NSTail(methodName), ' ') != NULL;
  } else {
    tailContainsSpace = 0;
  }
  /*fprintf(stderr, "<%s> containsSpace %d tailContainsSpace %d\n", methodName, containsSpace, tailContainsSpace);*/

#if !defined(NDEBUG)
  if (containsSpace != 0) {
    assert(strchr(methodName, ' ') != 0);
  } else {
    assert(tailContainsSpace == 0);
  }
#endif

  if (tailContainsSpace != 0) {
    const char *firstElementString;
    Tcl_Namespace *parentNsPtr;
    NsfObject *ensembleObject;
    Tcl_Obj *methodHandleObj, **ov;
    int oc, i;

    /*
     * When the methodName is required, we have to provide a methodNameDS as
     * well.
     */
    assert(methodName1 == NULL || methodNameDs != NULL);

    /*fprintf(stderr, "name '%s' contains space \n", methodName);*/

    if (likely(Tcl_ListObjGetElements(interp, methodObj, &oc, &ov) != TCL_OK)
        || ((referencedObject = GetEnsembleObjectFromName(interp, nsPtr, ov[0],
                                                          &cmd, fromClassNS)) == NULL)
        ) {
      if (methodName1 != NULL) {*methodName1 = NULL;}
      if (regObject != NULL) {*regObject = NULL;}
      if (defObject != NULL) {*defObject = NULL;}
      return NULL;
    }

    /*
     * We have an ensemble object. First, figure out, on which
     * object/class the ensemble object was registered. We determine
     * the regObject on the first element of the list. If we can't,
     * then the current object is the regObject.
     */
    firstElementString = ObjStr(ov[0]);
    if (*firstElementString == ':') {
      NsfObject *registrationObject;
      registrationObject = GetRegObject(interp, cmd, firstElementString, methodName1, fromClassNS);
      if (regObject != NULL) {*regObject = registrationObject;}
    } else {
      if (regObject != NULL) {*regObject = NULL;}
    }

    /*fprintf(stderr, "... regObject object '%s' reg %p, fromClassNS %d\n",
      ObjectName(referencedObject), *regObject, *fromClassNS);*/

    /*
     * Build a fresh methodHandleObj to held method name and names of
     * subcmds.
     */
    methodHandleObj = Tcl_DuplicateObj(referencedObject->cmdName);
    INCR_REF_COUNT(methodHandleObj);

    if (methodNameDs != NULL) {
      Tcl_DStringAppend(methodNameDs, Tcl_GetCommandName(interp, cmd), -1);
    }
    parentNsPtr = NULL;

    /*
     * Iterate over the objects and append to the methodNameDs and methodHandleObj
     */
    for (i = 1; i < oc; i++) {
      cmd = Tcl_GetCommandFromObj(interp, methodHandleObj);
      ensembleObject = (cmd != NULL) ? NsfGetObjectFromCmdPtr(cmd) : NULL;

      if (ensembleObject == NULL) {
        DECR_REF_COUNT(methodHandleObj);
        if (methodName1 != NULL) {*methodName1 = NULL;}
        if (regObject != NULL) {*regObject = NULL;}
        if (defObject != NULL) {*defObject = NULL;}
        return NULL;
      }

      if (parentNsPtr != NULL
          && (Tcl_Command_nsPtr(ensembleObject->id) != parentNsPtr)) {
        /* fprintf(stderr, "*** parent change saved parent %p %s computed parent %p %s\n",
                parentNsPtr, parentNsPtr->fullName,
                Tcl_Command_nsPtr(ensembleObject->id),
                Tcl_Command_nsPtr(ensembleObject->id)->fullName);*/
        DECR_REF_COUNT(methodHandleObj);
        methodHandleObj = Tcl_DuplicateObj(ensembleObject->cmdName);
      }
      parentNsPtr = ensembleObject->nsPtr;

      Tcl_AppendLimitedToObj(methodHandleObj, "::", 2, INT_MAX, NULL);
      Tcl_AppendLimitedToObj(methodHandleObj, ObjStr(ov[i]), -1, INT_MAX, NULL);
      if (methodNameDs != NULL) {
        Tcl_DStringAppendElement(methodNameDs, ObjStr(ov[i]));
      }
    }

    /*
     * cmd contains now the parent-obj, on which the method was
     * defined. Get from this cmd the defObj.
     */
    if (defObject != NULL) {*defObject = NsfGetObjectFromCmdPtr(cmd);}

    /*fprintf(stderr, "... handle '%s' last cmd %p defObject %p\n",
      ObjStr(methodHandleObj), cmd, *defObject);*/

    /*
     * Obtain the command from the method handle and report back the
     * final methodName,
     */
    cmd = Tcl_GetCommandFromObj(interp, methodHandleObj);
    if (methodNameDs != NULL && methodName1 != NULL) {
      *methodName1 = Tcl_DStringValue(methodNameDs);
    }

    /*fprintf(stderr, "... methodname1 '%s' cmd %p\n", Tcl_DStringValue(methodNameDs), cmd);*/
    DECR_REF_COUNT(methodHandleObj);

  } else if (*methodName == ':') {
    cmd = Tcl_GetCommandFromObj(interp, methodObj);
    if (likely(cmd != NULL)) {
      referencedObject = GetRegObject(interp, cmd, methodName, methodName1, fromClassNS);
      if (regObject != NULL) {*regObject = referencedObject;}
      if (defObject != NULL) {*defObject = referencedObject;}
      if (methodName1 && *methodName1 == NULL) {
        /*
         * The return value for the method name is required and was not
         * computed by GetRegObject()
         */
        *methodName1 = Tcl_GetCommandName(interp, cmd);
      }
    } else {
      /*
       * The cmd was not registered on an object or class, but we
       * still report back the cmd (might be e.g. a primitive cmd).
       */
      if (regObject != NULL) {*regObject = NULL;}
      if (defObject != NULL) {*defObject = NULL;}
    }
  } else {
    if (methodName1 != NULL) {*methodName1 = methodName;}
    cmd = (nsPtr != NULL) ? FindMethod(nsPtr, methodName) : NULL;
    if (regObject != NULL) {*regObject = NULL;}
    if (defObject != NULL) {*defObject = NULL;}
  }

  return cmd;
}

/*
 *----------------------------------------------------------------------
 * CmdIsProc --
 *
 *    Check, whether the cmd is interpreted
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int CmdIsProc(Tcl_Command cmd) nonnull(1);

NSF_INLINE static int
CmdIsProc(Tcl_Command cmd) {
  /* In 8.6: TclIsProc((Command *)cmd) is not equivalent to the definition below */
  nonnull_assert(cmd != NULL);
  return (Tcl_Command_objProc(cmd) == TclObjInterpProc);
}

/*
 *----------------------------------------------------------------------
 * CmdIsNsfObject --
 *
 *    Check whether the provided cmd refers to an NsfObject or Class.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int CmdIsNsfObject(Tcl_Command cmd) nonnull(1);

NSF_INLINE static int
CmdIsNsfObject(Tcl_Command cmd) {
  nonnull_assert(cmd != NULL);
  return Tcl_Command_objProc(cmd) == NsfObjDispatch;
}

/*
 *----------------------------------------------------------------------
 * GetTclProcFromCommand --
 *
 *    Check if cmd refers to a Tcl proc, and if so, return the proc
 *    definition.
 *
 * Results:
 *    The found proc of cmd or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static Proc *GetTclProcFromCommand(Tcl_Command cmd) nonnull(1);

static Proc *
GetTclProcFromCommand(Tcl_Command cmd) {
  Tcl_ObjCmdProc *proc;

  nonnull_assert(cmd != NULL);
  proc = Tcl_Command_objProc(cmd);
  if (proc == TclObjInterpProc) {
    return (Proc *)Tcl_Command_objClientData(cmd);
  }
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * FindMethod --
 *
 *    Lookup the cmd for methodName in a namespace.
 *
 * Results:
 *    The found cmd of the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

NSF_INLINE static Tcl_Command
FindMethod(Tcl_Namespace *nsPtr, const char *methodName) {
  register Tcl_HashEntry *entryPtr;

  nonnull_assert(nsPtr != NULL);
  nonnull_assert(methodName != NULL);

  if ((entryPtr = Tcl_CreateHashEntry(Tcl_Namespace_cmdTablePtr(nsPtr), methodName, NULL))) {
    return (Tcl_Command) Tcl_GetHashValue(entryPtr);
  }
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * FindProcMethod --
 *
 *    Lookup the proc for methodName in a namespace.
 *
 * Results:
 *    The found proc of the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static Proc * FindProcMethod(Tcl_Namespace *nsPtr, const char *methodName) nonnull(1) nonnull(2);

static Proc *
FindProcMethod(Tcl_Namespace *nsPtr, const char *methodName) {
  Tcl_Command cmd;

  nonnull_assert(nsPtr != NULL);
  nonnull_assert(methodName != NULL);

  cmd = FindMethod(nsPtr, methodName);
  return (cmd != NULL) ? GetTclProcFromCommand(cmd) : NULL;
}

/*
 *----------------------------------------------------------------------
 * SearchPLMethod, SearchPLMethod0 --
 *
 *    Search a method along a provided class list.  The methodName must be
 *    simple (must not contain space). While SearchPLMethod() allows to
 *    specify a flag for filtering the command, SearchPLMethod0() is a lightly
 *    optimized function without the filtering option.
 *
 * Results:
 *    The found class defining the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static NsfClass * SearchPLMethod(register NsfClasses *pl, const char *methodName, Tcl_Command *cmdPtr, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);
static NsfClass * SearchPLMethod0(register NsfClasses *pl, const char *methodName, Tcl_Command *cmdPtr)
  nonnull(1) nonnull(2) nonnull(3);

static NsfClass *
SearchPLMethod0(register NsfClasses *pl, const char *methodName, Tcl_Command *cmdPtr) {

  nonnull_assert(pl != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmdPtr != NULL);

  /* Search the precedence list (class hierarchy) */
  do {
    register Tcl_HashEntry *entryPtr =
      Tcl_CreateHashEntry(Tcl_Namespace_cmdTablePtr(pl->cl->nsPtr), methodName, NULL);
    if (entryPtr != NULL) {
      *cmdPtr = (Tcl_Command) Tcl_GetHashValue(entryPtr);
      return pl->cl;
    }
    pl = pl->nextPtr;
  } while (pl != NULL);

  return NULL;
}

static NsfClass *
SearchPLMethod(register NsfClasses *pl, const char *methodName,
               Tcl_Command *cmdPtr, unsigned int flags) {

  nonnull_assert(pl != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmdPtr != NULL);

  /* Search the precedence list (class hierarchy) */
  do {
    register Tcl_HashEntry *entryPtr =
      Tcl_CreateHashEntry(Tcl_Namespace_cmdTablePtr(pl->cl->nsPtr), methodName, NULL);
    if (entryPtr != NULL) {
      Tcl_Command cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);

      if (likely((Tcl_Command_flags(cmd) & flags) == 0u)) {
        *cmdPtr = cmd;
        return pl->cl;
      }
    }
    pl = pl->nextPtr;
  } while (pl != NULL);

  return NULL;
}

/*
 *----------------------------------------------------------------------
 * SearchCMethod --
 *
 *    Search a method along the superclass hierarchy of the provided
 *    class. The methodObj must be simple (must not contain
 *    space). The method has the interface for internal calls during
 *    interpretation, while SearchSimpleCMethod() has the interface
 *    with more overhead for introspection.
 *
 * Results:
 *    The found class defining the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static NsfClass * SearchCMethod(/*@notnull@*/ NsfClass *cl, const char *methodName, Tcl_Command *cmdPtr)
  nonnull(1) nonnull(2) nonnull(3);

static NsfClass *
SearchCMethod(/*@notnull@*/ NsfClass *cl, const char *methodName, Tcl_Command *cmdPtr) {

  nonnull_assert(methodName != NULL);
  nonnull_assert(cmdPtr != NULL);
  nonnull_assert(cl != NULL);

  return SearchPLMethod0(PrecedenceOrder(cl), methodName, cmdPtr);
}

/*
 *----------------------------------------------------------------------
 * SearchSimpleCMethod --
 *
 *    Search a method along the superclass hierarchy of the provided
 *    class. The methodObj must be simple (must not contain
 *    space). The method has the same interface as
 *    SearchComplexCMethod().
 *
 * Results:
 *    The found class defining the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static NsfClass * SearchSimpleCMethod(Tcl_Interp *interp, /*@notnull@*/ NsfClass *cl,
                    Tcl_Obj *methodObj, Tcl_Command *cmdPtr) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static NsfClass *
SearchSimpleCMethod(Tcl_Interp *interp, /*@notnull@*/ NsfClass *cl,
                    Tcl_Obj *methodObj, Tcl_Command *cmdPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(methodObj != NULL);
  nonnull_assert(cmdPtr != NULL);

  return SearchPLMethod0(PrecedenceOrder(cl), ObjStr(methodObj), cmdPtr);
}

/*
 *----------------------------------------------------------------------
 * SearchComplexCMethod --
 *
 *    Search a method along the superclass hierarchy of the provided
 *    class. The methodObj can refer to an ensemble object (can
 *    contain space). The method has the same interface as
 *    SearchSimpleCMethod().
 *
 * Results:
 *    The found class defining the method or NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static NsfClass * SearchComplexCMethod(Tcl_Interp *interp, /*@notnull@*/ NsfClass *cl,
                     Tcl_Obj *methodObj, Tcl_Command *cmdPtr) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static NsfClass *
SearchComplexCMethod(Tcl_Interp *interp, /*@notnull@*/ NsfClass *cl,
                     Tcl_Obj *methodObj, Tcl_Command *cmdPtr) {
  NsfClasses *pl;
  int fromClassNS = 1;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(methodObj != NULL);
  nonnull_assert(cmdPtr != NULL);

  for (pl = PrecedenceOrder(cl); pl;  pl = pl->nextPtr) {
    Tcl_Command cmd = ResolveMethodName(interp, pl->cl->nsPtr, methodObj,
                                        NULL, NULL, NULL, NULL, &fromClassNS);
    if (cmd != NULL) {
      *cmdPtr = cmd;
      return pl->cl;
    }
  }

  return NULL;
}

/*
 *----------------------------------------------------------------------
 * ObjectFindMethod --
 *
 *    Find a method for a given object in the precedence path. The
 *    provided methodObj might be an ensemble object. This function
 *    tries to optimize access by calling different implementations
 *    for simple and ensemble method names.
 *
 * Results:
 *    Tcl command.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Command ObjectFindMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *methodObj, NsfClass **pcl)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static Tcl_Command
ObjectFindMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *methodObj, NsfClass **pcl) {
  Tcl_Command cmd = NULL;
  NsfClass *(*lookupFunction)(Tcl_Interp *interp, NsfClass *cl,
                              Tcl_Obj *methodObj, Tcl_Command *cmdPtr);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObj != NULL);
  nonnull_assert(pcl != NULL);

  if (strchr(ObjStr(methodObj), ' ') != NULL) {
    lookupFunction = SearchComplexCMethod;
  } else {
    lookupFunction = SearchSimpleCMethod;
  }

  if (unlikely(object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
    MixinComputeDefined(interp, object);
  }

  if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
    NsfCmdList *mixinList;
    for (mixinList = object->mixinOrder; mixinList; mixinList = mixinList->nextPtr) {
      NsfClass *mixin = NsfGetClassFromCmdPtr(mixinList->cmdPtr);
      if (mixin && (*pcl = (*lookupFunction)(interp, mixin, methodObj, &cmd))) {
        if (Tcl_Command_flags(cmd) & NSF_CMD_CLASS_ONLY_METHOD && !NsfObjectIsClass(object)) {
          cmd = NULL;
          continue;
        }
        break;
      }
    }
  }

  if (cmd == NULL && object->nsPtr) {
    int fromClassNS = 0;

    cmd = ResolveMethodName(interp, object->nsPtr, methodObj,
                            NULL, NULL, NULL, NULL, &fromClassNS);
  }

  if (cmd == NULL && object->cl) {
    *pcl = (*lookupFunction)(interp, object->cl, methodObj, &cmd);
  }

  return cmd;
}

/*
 *----------------------------------------------------------------------
 * GetObjectSystem --
 *
 *    Return the object system for which the object was defined
 *
 * Results:
 *    Object system pointer
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfObjectSystem * GetObjectSystem(NsfObject *object) nonnull(1);

static NsfObjectSystem *
GetObjectSystem(NsfObject *object) {
  nonnull_assert(object != NULL);
  if (NsfObjectIsClass(object)) {
    return ((NsfClass *)object)->osPtr;
  }
  assert(object->cl != NULL);
  return object->cl->osPtr;
}

/*
 *----------------------------------------------------------------------
 * ObjectSystemFree --
 *
 *    Free a single object system structure including its root classes.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free memory of structure, free the root classes.
 *
 *----------------------------------------------------------------------
 */

static void ObjectSystemFree(Tcl_Interp *interp, NsfObjectSystem *osPtr) nonnull(1) nonnull(2);

static void
ObjectSystemFree(Tcl_Interp *interp, NsfObjectSystem *osPtr) {
  int idx;

  nonnull_assert(interp != NULL);
  nonnull_assert(osPtr != NULL);

  for (idx = 0; idx <= NSF_s_set_idx; idx++) {
    if (osPtr->methods[idx]) { DECR_REF_COUNT(osPtr->methods[idx]); }
    if (osPtr->handles[idx]) { DECR_REF_COUNT(osPtr->handles[idx]); }
  }

  if (osPtr->rootMetaClass != NULL && osPtr->rootClass != NULL) {
    RemoveSuper(osPtr->rootMetaClass, osPtr->rootClass);
    RemoveInstance((NsfObject *)osPtr->rootMetaClass, osPtr->rootMetaClass);
    RemoveInstance((NsfObject *)osPtr->rootClass, osPtr->rootMetaClass);

    FinalObjectDeletion(interp, &osPtr->rootClass->object);
    FinalObjectDeletion(interp, &osPtr->rootMetaClass->object);
  }

  FREE(NsfObjectSystem, osPtr);
}

/*
 *----------------------------------------------------------------------
 * ObjectSystemAdd --
 *
 *    Add and entry to the list of object systems of the interpreter.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Updating the per interp list of object systems.
 *
 *----------------------------------------------------------------------
 */
static void ObjectSystemAdd(Tcl_Interp *interp, NsfObjectSystem *osPtr) nonnull(1) nonnull(2);

static void
ObjectSystemAdd(Tcl_Interp *interp, NsfObjectSystem *osPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(osPtr != NULL);

  osPtr->nextPtr = RUNTIME_STATE(interp)->objectSystems;
  RUNTIME_STATE(interp)->objectSystems = osPtr;
}



/*
 *----------------------------------------------------------------------
 * ObjectSystemsCleanup --
 *
 *    Delete all objects from all defined object systems.  This method
 *    is to be called when an Next Scripting process or thread exists.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    All commands and objects are deleted, memory is freed.
 *
 *----------------------------------------------------------------------
 */
static int ObjectSystemsCleanup(Tcl_Interp *interp, int withKeepvars) nonnull(1);

static int
ObjectSystemsCleanup(Tcl_Interp *interp, int withKeepvars) {
  NsfCmdList *instances = NULL, *entryPtr;
  NsfObjectSystem *osPtr, *nPtr;

  nonnull_assert(interp != NULL);

  /* Deletion is performed in two rounds:
   *  (a) SOFT DESTROY: invoke all user-defined destroy methods
   *      without destroying objects
   *  (b) PHYSICAL DESTROY: delete the objects and classes,
   *      destroy methods are not invoked anymore
   *
   * This is to prevent that the destroy order causes classes to be
   * deleted before the methods invoked by destroy are executed.  Note
   * that it is necessary to iterate over all object systems
   * simultaneous, since the might be dependencies between objects of
   * different object systems.
   */

  /*
   * Collect all instances from all object systems
   */

  for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = osPtr->nextPtr) {
    GetAllInstances(interp, &instances, osPtr->rootClass);
  }

  /***** SOFT DESTROY *****/
  RUNTIME_STATE(interp)->exitHandlerDestroyRound = NSF_EXITHANDLER_ON_SOFT_DESTROY;

  /*fprintf(stderr, "===CALL destroy on OBJECTS\n");*/

  for (entryPtr = instances; entryPtr; entryPtr = entryPtr->nextPtr) {
    NsfObject *object = (NsfObject *)entryPtr->clorobj;

    /*fprintf(stderr, "key = %s %p %d flags %.6x\n",
      ObjectName(object), object, object && !NsfObjectIsClass(object), object->flags);*/

    if (object != NULL && !NsfObjectIsClass(object)
        && ((object->flags & NSF_DESTROY_CALLED) == 0u)) {
      DispatchDestroyMethod(interp, object, 0);
    }
  }

  /*fprintf(stderr, "===CALL destroy on CLASSES\n");*/

  for (entryPtr = instances; entryPtr; entryPtr = entryPtr->nextPtr) {
    NsfClass *cl = entryPtr->clorobj;

    if (cl != NULL && ((cl->object.flags & NSF_DESTROY_CALLED) == 0u)) {
      DispatchDestroyMethod(interp, (NsfObject *)cl, 0);
    }
  }

  /* now, turn of filters, all destroy callbacks are done */
  RUNTIME_STATE(interp)->doFilters = 0;

#ifdef DO_CLEANUP
  FreeAllNsfObjectsAndClasses(interp, &instances);
# ifdef DO_FULL_CLEANUP
  DeleteProcsAndVars(interp, Tcl_GetGlobalNamespace(interp), withKeepvars);
# endif
#endif

  /* now free all objects systems with their root classes */
  for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = nPtr) {
    nPtr = osPtr->nextPtr;
    ObjectSystemFree(interp, osPtr);
  }

#ifdef DO_CLEANUP
  /* finally, free all nsfprocs */
  DeleteNsfProcs(interp, NULL);
#endif

  CmdListFree(&instances, NULL);

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * CallDirectly --
 *
 *    Determine when it is possible/necessary to call a method
 *    implementation directly or via method dispatch.
 *
 * Results:
 *    1 is returned when command should be invoked directly, 0
 *    otherwise.
 *
 * Side effects:
 *    methodObjPtr is set with the Tcl_Obj of the name of the method,
 *    if there is one defined.
 *
 *----------------------------------------------------------------------
 */
static int CallDirectly(Tcl_Interp *interp, NsfObject *object, int methodIdx, Tcl_Obj **methodObjPtr)
  nonnull(1) nonnull(2) nonnull(4);

static int
CallDirectly(Tcl_Interp *interp, NsfObject *object, int methodIdx, Tcl_Obj **methodObjPtr) {
  /*
   * We can/must call a C-implemented method directly, when
   *
   *   a) the object system has no such appropriate method defined
   *
   *   b) the script does not contain a method with the appropriate
   *     name, and
   *
   *   c) filters are not active on the object
   */
  NsfObjectSystem *osPtr = GetObjectSystem(object);
  int callDirectly = 1;
  Tcl_Obj *methodObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObjPtr != NULL);

  methodObj = osPtr->methods[methodIdx];
  /*fprintf(stderr, "OS of %s is %s, method %s methodObj %p osPtr %p defined %.8x %.8x overloaded %.8x %.8x flags %.8x\n",
          ObjectName(object), ObjectName(&osPtr->rootClass->object),
          Nsf_SystemMethodOpts[methodIdx]+1, methodObj,
          osPtr,
          osPtr->definedMethods, osPtr->definedMethods & (1 << methodIdx),
          osPtr->overloadedMethods, osPtr->overloadedMethods & (1 << methodIdx),
          1 << methodIdx );*/

  if (methodObj != NULL) {
    unsigned int flag = 1 << methodIdx;

    if ((osPtr->overloadedMethods & flag) != 0u) {
      /* overloaded, we must dispatch */
      /*fprintf(stderr, "overloaded\n");*/
      callDirectly = 0;
    } else if ((osPtr->definedMethods & flag) == 0u) {
      /* not defined, we must call directly */
      /*fprintf(stderr, "Warning: CallDirectly object %s idx %s not defined\n",
        ObjectName(object), Nsf_SystemMethodOpts[methodIdx]+1);*/
    } else {
#if DISPATCH_ALWAYS_DEFINED_METHODS
      callDirectly = 0;
#else
      if ((object->flags & NSF_FILTER_ORDER_VALID) == 0u) {
        FilterComputeDefined(interp, object);
      }
      /*fprintf(stderr, "CallDirectly object %s idx %s object flags %.6x %.6x \n",
              ObjectName(object), Nsf_SystemMethodOpts[methodIdx]+1,
              (object->flags & NSF_FILTER_ORDER_DEFINED_AND_VALID),
              NSF_FILTER_ORDER_DEFINED_AND_VALID);*/
      if ((object->flags & NSF_FILTER_ORDER_DEFINED_AND_VALID) == NSF_FILTER_ORDER_DEFINED_AND_VALID) {
        /*fprintf(stderr, "CallDirectly object %s idx %s has filter \n",
          ObjectName(object), Nsf_SystemMethodOpts[methodIdx]+1);*/
        callDirectly = 0;
      }
#endif
    }
  }

  /*fprintf(stderr, "CallDirectly object %s idx %d returns %s => %d\n",
          ObjectName(object), methodIdx, (methodObj != NULL) ? ObjStr(methodObj) : "(null)", callDirectly);*/

  /* return the methodObj in every case */
  *methodObjPtr = methodObj;
  return callDirectly;
}

/*
 *----------------------------------------------------------------------
 * NsfMethodObj --
 *
 *    Return the methodObj for a given method index.
 *
 * Results:
 *    Returns Tcl_Obj* or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj * NsfMethodObj(NsfObject *object, int methodIdx) nonnull(1);

Tcl_Obj *
NsfMethodObj(NsfObject *object, int methodIdx) {
  NsfObjectSystem *osPtr = GetObjectSystem(object);

  nonnull_assert(object != NULL);
  /*
  fprintf(stderr, "NsfMethodObj object %s os %p idx %d %s methodObj %p\n",
          ObjectName(object), osPtr, methodIdx,
          Nsf_SystemMethodOpts[methodIdx]+1,
          osPtr->methods[methodIdx]);
  */
  return osPtr->methods[methodIdx];
}


/*
 * conditional memory allocations of optional storage
 */


static NsfObjectOpt *
NsfRequireObjectOpt(NsfObject *object) {

  nonnull_assert(object != NULL);

  if (object->opt == NULL) {
    object->opt = NEW(NsfObjectOpt);
    memset(object->opt, 0, sizeof(NsfObjectOpt));
  }
  return object->opt;
}


NsfClassOpt *
NsfRequireClassOpt(/*@notnull@*/ NsfClass *cl) {

  nonnull_assert(cl != NULL);

  if (cl->opt == NULL) {
    cl->opt = NEW(NsfClassOpt);
    memset(cl->opt, 0, sizeof(NsfClassOpt));
    if ((cl->object.flags & NSF_IS_CLASS) != 0u) {
      cl->opt->id = cl->object.id;  /* probably a temporary solution */
    }
  }
  return cl->opt;
}


static void MakeObjNamespace(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static void
MakeObjNamespace(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#ifdef NAMESPACE_TRACE
  fprintf(stderr, "+++ MakeObjNamespace for %s\n", ObjectName(object));
#endif
  if (object->nsPtr == NULL) {
    Tcl_Namespace *nsPtr;
    nsPtr = object->nsPtr = NSGetFreshNamespace(interp, object,
                                                ObjStr((object)->cmdName));
    assert(nsPtr != NULL);

    /*
     * Copy all obj variables to the newly created namespace
     */
    if (object->varTablePtr != NULL) {
      Tcl_HashSearch  search;
      Tcl_HashEntry   *hPtr;
      TclVarHashTable *varTablePtr = Tcl_Namespace_varTablePtr(nsPtr);
      Tcl_HashTable   *varHashTablePtr = TclVarHashTablePtr(varTablePtr);
      Tcl_HashTable   *objHashTablePtr = TclVarHashTablePtr(object->varTablePtr);

      *varHashTablePtr = *objHashTablePtr; /* copy the table */

      if (objHashTablePtr->buckets == objHashTablePtr->staticBuckets) {
        varHashTablePtr->buckets = varHashTablePtr->staticBuckets;
      }
      for (hPtr = Tcl_FirstHashEntry(varHashTablePtr, &search); hPtr;
           hPtr = Tcl_NextHashEntry(&search)) {
        hPtr->tablePtr = varHashTablePtr;
      }
      CallStackReplaceVarTableReferences(interp, object->varTablePtr,
                                         (TclVarHashTable *)varHashTablePtr);

      ckfree((char *) object->varTablePtr);
      object->varTablePtr = NULL;
    }
  }
}

static Tcl_Var CompiledLocalsLookup(CallFrame *varFramePtr, const char *varName) nonnull(1) nonnull(2);

static Tcl_Var
CompiledLocalsLookup(CallFrame *varFramePtr, const char *varName) {
  int i, localCt;
  Tcl_Obj **objPtrPtr;

  nonnull_assert(varFramePtr != NULL);
  nonnull_assert(varName != NULL);

  localCt = varFramePtr->numCompiledLocals;
  objPtrPtr = &varFramePtr->localCachePtr->varName0;

  /* fprintf(stderr, ".. search #local vars %d for %s\n", localCt, varName);*/
  for (i = 0 ; i < localCt ; i++, objPtrPtr++) {
    Tcl_Obj *objPtr = *objPtrPtr;
    if (likely(objPtr != NULL)) {
      const char *localName = TclGetString(objPtr);
      if (unlikely(varName[0] == localName[0]
                   && varName[1] == localName[1]
                   && strcmp(varName, localName) == 0)) {
        return (Tcl_Var) &varFramePtr->compiledLocals[i];
      }
    }
  }
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * GetVarAndNameFromHash --
 *
 *    Conveniance function to obtain variable and name from
 *    a variable hash entry
 *
 * Results:
 *    Results are passed back in argument 2 and 3
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void GetVarAndNameFromHash(Tcl_HashEntry *hPtr, Var **val, Tcl_Obj **varNameObj)
  nonnull(1) nonnull(2) nonnull(3);

static void
GetVarAndNameFromHash(Tcl_HashEntry *hPtr, Var **val, Tcl_Obj **varNameObj) {

  nonnull_assert(hPtr != NULL);
  nonnull_assert(val != NULL);
  nonnull_assert(varNameObj != NULL);

  *val = TclVarHashGetValue(hPtr);
  *varNameObj = TclVarHashGetKey(*val);
}


/*********************************************************
 *
 * Variable resolvers
 *
 *********************************************************/
#define FOR_COLON_RESOLVER(ptr) (*(ptr) == ':' && *((ptr)+1) != ':')

/*
 *----------------------------------------------------------------------
 * MethodName --
 *
 *    Return the methodName from a Tcl_Obj, strips potentially the
 *    colon prefix
 *
 * Results:
 *    method name
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static const char *MethodName(Tcl_Obj *methodObj) nonnull(1) returns_nonnull;

static const char *
MethodName(Tcl_Obj *methodObj) {
  const char *methodName;

  nonnull_assert(methodObj != NULL);

  methodName = ObjStr(methodObj);
  if (FOR_COLON_RESOLVER(methodName)) {
    methodName ++;
  }
  return methodName;
}

const char *
NsfMethodName(Tcl_Obj *methodObj) {

  nonnull_assert(methodObj != NULL);

  return MethodName(methodObj);
}

/*
 *----------------------------------------------------------------------
 * NsfMethodNamePath --
 *
 *    Compute the full method name for error messages containing the
 *    ensemble root.
 *
 * Results:
 *    Tcl_Obj, caller has to take care for ref-counting
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
NsfMethodNamePath(Tcl_Interp *interp,
                  Tcl_CallFrame *framePtr,
                  const char *methodName) {

  Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);

  if (framePtr != NULL) {
    Tcl_ListObjAppendList(interp, resultObj,
                          CallStackMethodPath(interp, framePtr));
  }

  Tcl_ListObjAppendElement(interp, resultObj,
                           Tcl_NewStringObj(methodName, -1));
  return resultObj;
}


/*
 *----------------------------------------------------------------------
 * NsColonVarResolver --
 *
 *    Namespace resolver for namespace specific variable lookup.
 *    colon prefix
 *
 * Results:
 *    method name
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int NsColonVarResolver(Tcl_Interp *interp, const char *varName, Tcl_Namespace *UNUSED(nsPtr),
                              int flags, Tcl_Var *varPtr)
  nonnull(1) nonnull(2) nonnull(5);

static int
NsColonVarResolver(Tcl_Interp *interp, const char *varName, Tcl_Namespace *UNUSED(nsPtr),
                   int flags, Tcl_Var *varPtr) {
  Tcl_CallFrame *varFramePtr;
  TclVarHashTable *varTablePtr;
  NsfObject *object;
  int new, frameFlags;
  Tcl_Obj *key;

  nonnull_assert(interp != NULL);
  nonnull_assert(varName != NULL);
  nonnull_assert(varPtr != NULL);

#if defined (VAR_RESOLVER_TRACE)
  fprintf(stderr, "NsColonVarResolver '%s' flags %.6x\n", varName, flags);
#endif

  /*
   * Case 1: The variable is to be resolved in global scope, proceed in
   * resolver chain
   */
  if (unlikely((flags & TCL_GLOBAL_ONLY) != 0u)) {
    /*fprintf(stderr, "global-scoped lookup for var '%s' in NS '%s'\n", varName,
      nsPtr->fullName);*/
    return TCL_CONTINUE;
  }

  /*
   * Case 2: The lookup happens in a proc frame (lookup in compiled
   * locals and hash-table vars).  We are not interested to handle
   * these cases here, so proceed in resolver chain.
   */
  varFramePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp);
  assert(varFramePtr != NULL);

  frameFlags = Tcl_CallFrame_isProcCallFrame(varFramePtr);
#if defined (VAR_RESOLVER_TRACE)
  fprintf(stderr, "NsColonVarResolver '%s' frame flags %.6x\n", varName,
          Tcl_CallFrame_isProcCallFrame(varFramePtr));
#endif

  if ((frameFlags & FRAME_IS_PROC) != 0u) {
#if defined (VAR_RESOLVER_TRACE)
    fprintf(stderr, "...... forwarding to next resolver\n");
#endif
    /*fprintf(stderr, "proc-scoped var '%s' assumed, frame %p flags %.6x\n",
      name, varFramePtr, Tcl_CallFrame_isProcCallFrame(varFramePtr));*/
    return TCL_CONTINUE;
  }

  /*
   * FRAME_IS_NSF_CMETHOD has always FRAME_IS_PROC set, so it is
   * handled already above
   */
  assert((frameFlags & FRAME_IS_NSF_CMETHOD) == 0u);

  if ((frameFlags & FRAME_IS_NSF_OBJECT) == 0u) {
    /*
     * Case 3: we are not in an Next Scripting frame, so proceed as well
     */
    return TCL_CONTINUE;

  } else {
    /*
     *  Case 4: we are in an Next Scripting object frame
     */

    if (*varName == ':') {
      if (*(varName+1) != ':') {
        /*
         * Case 4a: The variable name starts with a single ":". Skip
         * the char, but stay in the resolver.
         */
        varName ++;
      } else {
        /*
         * Case 4b: Names starting  with "::" are not for us
         */
        return TCL_CONTINUE;
      }
    } else if (NSTail(varName) != varName) {
      /*
       * Case 4c: Names containing "::" are not for us
       */
      return TCL_CONTINUE;
    }

    /*
     * Since we know that we are here always in an object frame, we
     * can blindly get the object from the client data .
     */
    object = (NsfObject *)Tcl_CallFrame_clientData(varFramePtr);
  }

  /*
   * We have an object and create the variable if not found
   */
  assert(object != NULL);

  varTablePtr = (object->nsPtr != NULL) ? Tcl_Namespace_varTablePtr(object->nsPtr) : object->varTablePtr;
  assert(varTablePtr != NULL);

  /*
   * Does the variable exist in the object's namespace?
   */
  key = Tcl_NewStringObj(varName, -1);
  INCR_REF_COUNT(key);

  *varPtr = (Tcl_Var)VarHashCreateVar(varTablePtr, key, NULL);

#if defined (VAR_RESOLVER_TRACE)
  fprintf(stderr, "...... lookup of '%s' for object '%s' returns %p\n",
          varName, ObjectName(object), *varPtr);
#endif
  if (*varPtr == NULL) {
    /*
     * We failed to find the variable so far, therefore we create it
     * in this var table.  Note that in several cases above,
     * TCL_CONTINUE takes care for variable creation.
     */

    Var *newVar = VarHashCreateVar(varTablePtr, key, &new);
    *varPtr = (Tcl_Var)newVar;
  }
  DECR_REF_COUNT(key);

  return likely(*varPtr != NULL) ? TCL_OK : TCL_ERROR;
}

/*********************************************************
 *
 * Begin of compiled var resolver
 *
 *********************************************************/

typedef struct NsfResolvedVarInfo {
  Tcl_ResolvedVarInfo vInfo;        /* This must be the first element. */
  NsfObject *lastObject;
  Tcl_Var var;
  Tcl_Obj *nameObj;
} NsfResolvedVarInfo;

/*
 *----------------------------------------------------------------------
 * HashVarFree --
 *
 *    Free hashed variables based on refCount.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *   Changed refCount or freed variable.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static void
HashVarFree(Tcl_Var var) {
  if (unlikely(VarHashRefCount(var) < 2)) {
    /*fprintf(stderr,"#### free %p\n", var);*/
    ckfree((char *) var);
  } else {
    VarHashRefCount(var)--;
  }
}

/*
 *----------------------------------------------------------------------
 * CompiledColonVarFetch --
 *
 *    This function is the actual variable resolution handler for a
 *    colon-prefixed (":/varName/") found in a compiled script
 *    registered by the compiling var resolver (see
 *    InterpCompiledColonResolver()). When initializing a call frame,
 *    this handler is called, crawls the object's var table (creating
 *    a variable, if needed), and returns a Var structure. Based on
 *    this, a link variable ":/varName/" pointing to this object
 *    variable (i.e., "varName") is created and is stored in the
 *    compiled locals array of the call frame. Beware that these link
 *    variables interact with the family of link-creating commands
 *    ([variable], [global], [upvar]) by being subject to
 *    "retargeting" upon name conflicts (see
 *    tests/varresolutiontest.tcl for some examples).
 *
 * Results:
 *    Tcl_Var containing value or NULL.
 *
 * Side effects:
 *    Updates of Variable structure cache in necessary.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Var CompiledColonVarFetch(Tcl_Interp *interp, Tcl_ResolvedVarInfo *vinfoPtr) nonnull(1) nonnull(2);

static Tcl_Var
CompiledColonVarFetch(Tcl_Interp *interp, Tcl_ResolvedVarInfo *vinfoPtr) {
  NsfResolvedVarInfo *resVarInfo;
  NsfCallStackContent *cscPtr;
  NsfObject *object;
  TclVarHashTable *varTablePtr;
  Tcl_Var var;
  int new;

  nonnull_assert(interp != NULL);
  nonnull_assert(vinfoPtr != NULL);

  resVarInfo = (NsfResolvedVarInfo *)vinfoPtr;
  var = resVarInfo->var;

#if defined(VAR_RESOLVER_TRACE)
  unsigned int flags = (var != NULL) ? ((Var *)var)->flags : 0;
  fprintf(stderr,"CompiledColonVarFetch var '%s' var %p flags = %.4x dead? %.4x\n",
          ObjStr(resVarInfo->nameObj), var, flags, flags & VAR_DEAD_HASH);
#endif

  cscPtr = CallStackGetTopFrame0(interp);
  if (likely(cscPtr != NULL)) {
    object = cscPtr->self;
  } else {
    object = NULL;
  }

  /*
   * We cache lookups based on nsf objects; we have to care about
   * cases, where the instance variables are in some delete states.
   *
   */

  if ((var != NULL
       && object == resVarInfo->lastObject
       && (((((Var *)var)->flags) & VAR_DEAD_HASH)) == 0u)) {
    /*
     * The variable is valid.
     */
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, ".... cached var '%s' var %p flags = %.4x\n",
            ObjStr(resVarInfo->nameObj), var, ((Var *)var)->flags);
#endif
    return var;
  }

  if (unlikely(object == NULL)) {
    return NULL;
  }

  if (var != NULL) {
    /*
     * The variable is not valid anymore. Clean it up.
     */
    HashVarFree(var);
  }

  if (object->nsPtr != NULL) {
    varTablePtr = Tcl_Namespace_varTablePtr(object->nsPtr);
  } else if (object->varTablePtr != NULL) {
    varTablePtr = object->varTablePtr;
  } else {
    /*
     * In most situations, we have a varTablePtr through the clauses
     * above. However, if someone redefines e.g. the method "configure" or
     * "objectparameter", we might find an object with an still empty
     * varTable, since these are lazy initiated.
     */
    varTablePtr = object->varTablePtr = VarHashTableCreate();
  }
  assert(varTablePtr != NULL);

  resVarInfo->lastObject = object;
#if defined(VAR_RESOLVER_TRACE)
  fprintf(stderr,"Fetch var %s in object %s\n", TclGetString(resVarInfo->nameObj), ObjectName(object));
#endif
  resVarInfo->var = var = (Tcl_Var) VarHashCreateVar(varTablePtr, resVarInfo->nameObj, &new);
  /*
   * Increment the reference counter to avoid ckfree() of the variable
   * in Tcl's FreeVarEntry(); for cleanup, we provide our own
   * HashVarFree();
   */
  VarHashRefCount(var)++;
#if defined(VAR_RESOLVER_TRACE)
  {
    Var *v = (Var *)(resVarInfo->var);
    fprintf(stderr, ".... looked up existing var %s var %p flags = %.6x undefined %d\n",
            ObjStr(resVarInfo->nameObj),
            v, v->flags,
            TclIsVarUndefined(v));
  }
#endif
  return var;
}

/*
 *----------------------------------------------------------------------
 * CompiledColonVarFree --
 *
 *    DeleteProc of the compiled variable handler.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *   Free compiled variable structure and variable.
 *
 *----------------------------------------------------------------------
 */
static void CompiledColonVarFree(Tcl_ResolvedVarInfo *vInfoPtr) nonnull(1);

static void
CompiledColonVarFree(Tcl_ResolvedVarInfo *vInfoPtr) {
  NsfResolvedVarInfo *resVarInfo;

  nonnull_assert(vInfoPtr != NULL);

  resVarInfo = (NsfResolvedVarInfo *)vInfoPtr;
#if defined(VAR_RESOLVER_TRACE)
  fprintf(stderr, "CompiledColonVarFree %p for variable '%s'\n",
          resVarInfo, ObjStr(resVarInfo->nameObj));
#endif

  DECR_REF_COUNT(resVarInfo->nameObj);
  if (resVarInfo->var != NULL) {HashVarFree(resVarInfo->var);}
  FREE(NsfResolvedVarInfo, vInfoPtr);
}

/*
 *----------------------------------------------------------------------
 * InterpCompiledColonVarResolver --
 *
 *    For colon-prefixed (":/varName/") variables, we provide our own
 *    var resolver for compiling scripts and evaluating compiled
 *    scripts (e.g., proc bodies). At the time of first compilation
 *    (or re-compilation), this resolver is processed (see
 *    tclProc.c:InitResolvedLocals()). It registers two handlers for a
 *    given, colon-prefixed variable found in the script: the actual
 *    variable fetcher and a variable cleanup handler. The variable
 *    fetcher is executed whenever a Tcl call frame is initialized and
 *    the array of compiled locals is constructed (see also
 *    InitResolvedLocals()).
 *
 *    The Tcl var resolver protocol dictates that per-namespace
 *    compiling var resolvers take precedence over this per-interp
 *    compiling var resolver. That is, per-namespace resolvers are
 *    processed first and can effectively outrule per-interp
 *    resolvers by signaling TCL_OK or TCL_BREAK.
 *
 * Results:
 *    TCL_OK or TCL_CONTINUE (according to Tcl's var resolver protocol)
 *
 * Side effects:
 *    Registers per-variable resolution and cleanup handlers.
 *
 *----------------------------------------------------------------------
 */
static int InterpCompiledColonVarResolver(Tcl_Interp *interp, CONST84 char *name, int length,
                                          Tcl_Namespace *UNUSED(context), Tcl_ResolvedVarInfo **rPtr)
  nonnull(1) nonnull(2) nonnull(5);

static int
InterpCompiledColonVarResolver(Tcl_Interp *interp,
                               CONST84 char *name, int length, Tcl_Namespace *UNUSED(context),
                               Tcl_ResolvedVarInfo **rPtr) {
  /*
   *  The variable handler is registered, when we have an active Next Scripting
   *  object and the variable starts with the appropriate prefix. Note
   *  that getting the "self" object is a weak protection against
   *  handling of wrong vars
   */
  NsfObject *object = GetSelfObj(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  nonnull_assert(rPtr != NULL);

#if defined(VAR_RESOLVER_TRACE)
  fprintf(stderr, "compiled var resolver for %s, obj %p\n", name, object);
#endif

  if (likely(object != NULL) && FOR_COLON_RESOLVER(name)) {
    NsfResolvedVarInfo *resVarInfo = NEW(NsfResolvedVarInfo);

    resVarInfo->vInfo.fetchProc = CompiledColonVarFetch;
    resVarInfo->vInfo.deleteProc = CompiledColonVarFree; /* if NULL, Tcl does a ckfree on proc clean up */
    resVarInfo->lastObject = NULL;
    resVarInfo->var = NULL;
    resVarInfo->nameObj = Tcl_NewStringObj(name+1, length-1);
    INCR_REF_COUNT(resVarInfo->nameObj);

#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, "... resVarInfo %p nameObj %p '%s' obj %p %s\n",
            resVarInfo, resVarInfo->nameObj, ObjStr(resVarInfo->nameObj),
            object, ObjectName(object));
#endif

    *rPtr = (Tcl_ResolvedVarInfo *)resVarInfo;

    return TCL_OK;
  }
  return TCL_CONTINUE;
}

/*
 *----------------------------------------------------------------------
 * InterpGetFrameAndFlags --
 *
 *    Return for the provided interp the flags of the frame (returned as
 *    result) and the actual varFrame (returned in the second argument). In
 *    case, the toplevel frame is a LAMBDA frame, skip it.
 *
 * Results:
 *    Frame flags, varFrame
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

NSF_INLINE static int InterpGetFrameAndFlags(Tcl_Interp *interp, CallFrame **framePtr) nonnull(1) nonnull(2);

NSF_INLINE static int
InterpGetFrameAndFlags(Tcl_Interp *interp, CallFrame **framePtr) {
  int frameFlags;

  nonnull_assert(interp != NULL);
  nonnull_assert(framePtr != NULL);

  *framePtr = Tcl_Interp_varFramePtr(interp);
  frameFlags = Tcl_CallFrame_isProcCallFrame(*framePtr);
  /*
   * If the resolver is called from a lambda frame, use always the parent frame
   */
  if ((frameFlags & FRAME_IS_LAMBDA) != 0u) {
    *framePtr = (CallFrame *)Tcl_CallFrame_callerPtr(*framePtr);
    frameFlags = Tcl_CallFrame_isProcCallFrame(*framePtr);
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, "InterpColonVarResolver skip lambda frame flags %.6x\n",
            Tcl_CallFrame_isProcCallFrame(*framePtr));
#endif
  }
#if defined(VAR_RESOLVER_TRACE)
  fprintf(stderr, "... final frame flags %.6x\n", frameFlags);
#endif
  return frameFlags;
}

/*
 *----------------------------------------------------------------------
 * InterpColonVarResolver --
 *
 *    For accessing object (instance) variables using the colon-prefix
 *    notation (":/varName/"), we provide our own var resolvers. This
 *    function is the non-compiling var resolver; its services are
 *    requested in two situations: a) when evaluating non-compiled
 *    statements, b) when executing slow-path bytecode instructions,
 *    with "slow path" referring to bytecode instructions not making
 *    use of the compiled locals array (and, e.g., reverting to
 *    TclObjLookupVar*() calls).
 *
 *    The Tcl var resolver protocol dictates that per-namespace,
 *    non-compiling var resolvers take precedence over this per-interp
 *    non-compiling var resolver. That is, per-namespace resolvers are
 *    processed first and can effectively outrule per-interp resolvers
 *    by signaling TCL_OK or TCL_BREAK. See
 *    e.g. TclLookupSimpleVar().
 *
 * Results:
 *    TCL_OK or TCL_CONTINUE (according to on Tcl's var resolver protocol)
 *
 * Side effects:
 *    If successful, return varPtr, pointing to instance variable.
 *
 *----------------------------------------------------------------------
 */

static int InterpColonVarResolver(Tcl_Interp *interp, const char *varName, Tcl_Namespace *UNUSED(nsPtr),
                                  int flags, Tcl_Var *varPtr)
  nonnull(1) nonnull(2) nonnull(5);

static int
InterpColonVarResolver(Tcl_Interp *interp, const char *varName, Tcl_Namespace *UNUSED(nsPtr),
                       int flags, Tcl_Var *varPtr) {
  int new, frameFlags;
  CallFrame *varFramePtr;
  TclVarHashTable *varTablePtr;
  NsfObject *object;
  Tcl_Obj *keyObj;
  Tcl_Var var;

  nonnull_assert(interp != NULL);
  nonnull_assert(varName != NULL);
  nonnull_assert(varPtr != NULL);

  /*
   * TCL_GLOBAL_ONLY is removed, since "vwait :varName" is called with
   * with this flag.
   */
  if (!FOR_COLON_RESOLVER(varName) || (flags & (/*TCL_GLOBAL_ONLY|*/TCL_NAMESPACE_ONLY))) {
    /* ordinary names and global lookups are not for us */
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, "InterpColonVarResolver '%s' flags %.6x not for us\n",
            varName, flags);
#endif
    return TCL_CONTINUE;
  }

  frameFlags = InterpGetFrameAndFlags(interp, &varFramePtr);

  if (likely((frameFlags & FRAME_IS_NSF_METHOD) != 0u)) {
    if ((*varPtr = CompiledLocalsLookup(varFramePtr, varName))) {
      /*
       * This section is reached under notable circumstances and represents a
       * point of interaction between our resolvers for non-compiled (i.e.,
       * InterpColonVarResolver()) and compiled script execution (i.e.,
       * InterpCompiledColonVarResolver()).
       *
       * Expect this branch to be hit iff...
       *
       * 1. ... InterpCompiledColonVarResolver() is called from within the Tcl
       * bytecode interpreter when executing a bytecode-compiled script on a
       * *slow path* (i.e., involving a TclObjLookupVarEx() call)
       *
       * 2. ... the act of variable resolution (i.e., TclObjLookupVarEx()) has
       * not been restricted to the global (TCL_GLOBAL_ONLY) or an effective
       * namespace (TCL_NAMESPACE_ONLY)
       *
       * 3. ..., resulting from the fact of participating in an bytecode
       * interpretation, CompiledColonVarFetch() stored a link variable
       * (pointing to the actual/real object variable, whether defined or not)
       * under the given varName value into the current call frame's array of
       * compiled locals (when initializing the call frame; see
       * tclProc.c:InitResolvedLocals()).
       */
#if defined(VAR_RESOLVER_TRACE)
      fprintf(stderr, ".... found local %s varPtr %p flags %.6x\n",
              varName, *varPtr, flags);
#endif
      /*
       * By looking up the compiled-local directly and signaling TCL_OK, we
       * optimise a little by avoiding further lookups down the Tcl var
       * resolution infrastructure. Note that signaling TCL_CONTINUE would
       * work too, however, it would involve extra resolution overhead.
       */
      return TCL_OK;
    }

    object = ((NsfCallStackContent *)varFramePtr->clientData)->self;

  } else if ((frameFlags & FRAME_IS_NSF_CMETHOD) != 0u) {
    object = ((NsfCallStackContent *)varFramePtr->clientData)->self;

  } else if ((frameFlags & FRAME_IS_NSF_OBJECT) != 0u) {
    object = (NsfObject *)(varFramePtr->clientData);

  } else {
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, ".... not found %s\n", varName);
#endif
    return TCL_CONTINUE;
  }

  /*
   * Trim the varName for the colon prefix (":").
   */
  varName ++;

  /*
   * We have an object and create the variable if not found
   */
  assert(object != NULL);
  if (unlikely(object->nsPtr != NULL)) {
    varTablePtr = Tcl_Namespace_varTablePtr(object->nsPtr);
  } else if (likely(object->varTablePtr != NULL)) {
    varTablePtr = object->varTablePtr;
  } else {
    /*
     * In most situations, we have a varTablePtr through the clauses
     * above. However, if someone redefines e.g. the method "configure" or
     * "objectparameter", we might find an object with an still empty
     * varTable, since these are lazy initiated.
     */
    varTablePtr = object->varTablePtr = VarHashTableCreate();
  }
  assert(varTablePtr != NULL);

  /*fprintf(stderr, "Object Var Resolver, name=%s, obj %p, nsPtr %p, varTablePtr %p\n",
    varName, object, object->nsPtr, varTablePtr);*/

  keyObj = Tcl_NewStringObj(varName, -1);
  INCR_REF_COUNT(keyObj);

  var = (Tcl_Var)VarHashCreateVar(varTablePtr, keyObj, NULL);
  if (likely(var != NULL)) {
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, ".... found in hash-table %s %p flags %.6x ns %p\n",
            varName, var, ((Var *)var)->flags,  object->nsPtr);
#endif
    /* make coverage analysis easier */
    assert(1);
  } else {
    /*
     * We failed to find the variable, therefore we create it new
     */
    var = (Tcl_Var)VarHashCreateVar(varTablePtr, keyObj, &new);
#if defined(VAR_RESOLVER_TRACE)
    fprintf(stderr, ".... var %p %s created in hash-table %p\n", var, varName, varTablePtr);
#endif
  }

  *varPtr = var;
  DECR_REF_COUNT(keyObj);

  return TCL_OK;
}

/*********************************************************
 *
 * End of var resolvers
 *
 *********************************************************/

/*********************************************************
 *
 * Begin of cmd resolver
 *
 *********************************************************/
/*
 *----------------------------------------------------------------------
 * InterpColonCmdResolver --
 *
 *    Resolve command names. If the command starts with the Next
 *    Scripting specific prefix and we are on an Next Scripting stack
 *    frame, treat command as OO method.
 *
 * Results:
 *    TCL_OK or TCL_CONTINUE (based on Tcl's command resolver protocol)
 *
 * Side effects:
 *   If successful, return cmdPtr, pointing to method.
 *
 *----------------------------------------------------------------------
 */
static int InterpColonCmdResolver(Tcl_Interp *interp, const char *cmdName, Tcl_Namespace *UNUSED(nsPtr),
                                  unsigned int flags, Tcl_Command *cmdPtr) nonnull(1) nonnull(2) nonnull(5);

static int
InterpColonCmdResolver(Tcl_Interp *interp, const char *cmdName, Tcl_Namespace *UNUSED(nsPtr),
                       unsigned int flags, Tcl_Command *cmdPtr) {
  CallFrame *varFramePtr;
  int frameFlags;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(cmdPtr != NULL);

  /* fprintf(stderr, "InterpColonCmdResolver %s flags %.6x\n", cmdName, flags); */

  if (likely((*cmdName == ':' && *(cmdName + 1) == ':') || (flags & TCL_GLOBAL_ONLY) != 0u)) {
    /* fully qualified names and global lookups are not for us */
    /*fprintf(stderr, "... not for us %s flags %.6x\n", cmdName, flags);*/
    return TCL_CONTINUE;
  }

  frameFlags = InterpGetFrameAndFlags(interp, &varFramePtr);

  /*
   * The resolver is called as well, when a body of a method is
   * compiled.  In these situations, Tcl stacks a non-proc frame, that
   * we have to skip. In order to safely identify such situations, we
   * stuff into the call flags of the proc frame during the
   * compilation step NSF_CSC_CALL_IS_COMPILE.
   */
  if (frameFlags == 0 && Tcl_CallFrame_callerPtr(varFramePtr)) {
    varFramePtr = (CallFrame *)Tcl_CallFrame_callerPtr(varFramePtr);
    frameFlags = Tcl_CallFrame_isProcCallFrame(varFramePtr);

    if ((frameFlags & (FRAME_IS_NSF_METHOD)) == 0u
        || (((NsfCallStackContent *)varFramePtr->clientData)->flags & NSF_CSC_CALL_IS_COMPILE) == 0u
        ) {
      frameFlags = 0;
    } else {
#if defined(CMD_RESOLVER_TRACE)
      fprintf(stderr, "InterpColonCmdResolver got parent frame cmdName %s flags %.6x, frame flags %.6x\n",
              cmdName, flags, Tcl_CallFrame_isProcCallFrame(varFramePtr));
#endif
    }
 }

#if defined(CMD_RESOLVER_TRACE)
  fprintf(stderr, "InterpColonCmdResolver cmdName %s flags %.6x, frame flags %.6x\n",
          cmdName, flags, frameFlags);
#endif

  if ((frameFlags & (FRAME_IS_NSF_METHOD|FRAME_IS_NSF_OBJECT|FRAME_IS_NSF_CMETHOD)) != 0u) {
    if (*cmdName == ':') {
#if defined(CMD_RESOLVER_TRACE)
      fprintf(stderr, "    ... call colonCmd for %s\n", cmdName);
#endif
      /*
       * We have a cmd starting with ':', we are in an nsf frame, so
       * forward to the colonCmd.
       */
      *cmdPtr = RUNTIME_STATE(interp)->colonCmd;
      return TCL_OK;
    } else {

#if defined(NSF_WITH_OS_RESOLVER)
      /*
       * Experimental Object-System specific resolver: If an
       * un-prefixed method name is found in a body of a method, we try
       * to perform a lookup for this method in the namespace of the
       * object system for the current object. If this lookup is not
       * successful the standard lookups are performed. The
       * object-system specific resolver allows to use the "right"
       * (un-prefixed) "self" or "next" calls without namespace
       * imports.
       */
      NsfObject *object;
      NsfObjectSystem *osPtr;

      if ((frameFlags & (FRAME_IS_NSF_METHOD|FRAME_IS_NSF_CMETHOD)) != 0u) {
        object = ((NsfCallStackContent *)varFramePtr->clientData)->self;
      } else if ((frameFlags & (FRAME_IS_NSF_OBJECT)) != 0u) {
        object = (NsfObject *)(varFramePtr->clientData);
      } else {
        object = NULL;
      }
      if (object != NULL) {
        Tcl_HashEntry *entryPtr;
        Tcl_HashTable *cmdTablePtr;
        Tcl_Command cmd;

        osPtr = GetObjectSystem(object);
        cmd = osPtr->rootClass->object.id;
        cmdTablePtr = Tcl_Namespace_cmdTablePtr(((Command *)cmd)->nsPtr);
        entryPtr = Tcl_CreateHashEntry(cmdTablePtr, cmdName, NULL);
        /*fprintf(stderr, "InterpColonCmdResolver OS specific resolver tried to lookup %s for os %s in ns %s\n",
          cmdName, ClassName(osPtr->rootClass), ((Command *)cmd)->nsPtr->fullName);*/
        if (entryPtr != NULL) {
          /*fprintf(stderr, "InterpColonCmdResolver OS specific resolver found %s::%s\n",
            ((Command *)cmd)->nsPtr->fullName, cmdName);*/
          *cmdPtr = Tcl_GetHashValue(entryPtr);
          return TCL_OK;
        }
      }
#endif
    }
  }

#if defined(CMD_RESOLVER_TRACE)
  fprintf(stderr, "    ... not found %s\n", cmdName);
  NsfShowStack(interp);
#endif
  return TCL_CONTINUE;
}
/*********************************************************
 *
 * End of cmd resolver
 *
 *********************************************************/


/*
 *----------------------------------------------------------------------
 * NsfNamespaceInit --
 *
 *    Initialize a provided namespace by setting its resolvers and
 *    namespace path
 *
 * Results:
 *    none
 *
 * Side effects:
 *    change ns behavior
 *
 *----------------------------------------------------------------------
 */

static void NsfNamespaceInit(Tcl_Namespace *nsPtr) nonnull(1);

static void
NsfNamespaceInit(Tcl_Namespace *nsPtr) {

  nonnull_assert(nsPtr != NULL);

  /*
   * This puts a per-object namespace resolver into position upon
   * acquiring the namespace. Works for object-scoped commands/procs
   * and object-only ones (set, unset, ...)
   */
  Tcl_SetNamespaceResolvers(nsPtr,
                            (Tcl_ResolveCmdProc *)NULL,
                            NsColonVarResolver,
                            (Tcl_ResolveCompiledVarProc *)NULL);

#if defined(NSF_WITH_INHERIT_NAMESPACES)
  /*
   * In case there is a namespace path set for the parent namespace,
   * apply this as well to the object namespace to avoid surprises
   * with "namespace path nx".
   */
  { Namespace *parentNsPtr = Tcl_Namespace_parentPtr(nsPtr);
    int pathLength = Tcl_Namespace_commandPathLength(parentNsPtr);

    if (pathLength > 0) {
      Namespace **pathArray = (Namespace **)ckalloc(sizeof(Namespace *) * pathLength);
      NamespacePathEntry *tmpPathArray = Tcl_Namespace_commandPathArray(parentNsPtr);
      int i;

      for (i = 0; i < pathLength; i++) {
        pathArray[i] = tmpPathArray[i].nsPtr;
      }
      TclSetNsPath((Namespace *)nsPtr, pathLength, (Tcl_Namespace **)pathArray);
      ckfree((char *)pathArray);
    }
  }
#endif
}

static NsfObject *
NSNamespaceClientDataObject(ClientData clientData) {
#ifdef NSF_MEM_COUNT
  NsfNamespaceClientData *nsClientData = (NsfNamespaceClientData *)clientData;

  nonnull_assert(clientData != NULL);

  /*fprintf(stderr, "NSNamespaceDeleteProc cd %p\n", clientData);
    fprintf(stderr, "... nsPtr %p name '%s'\n", nsClientData->nsPtr, nsClientData->nsPtr->fullName);*/

  return nsClientData->object;
#else
  nonnull_assert(clientData != NULL);
  return (NsfObject *) clientData;
#endif
}


/*
 *----------------------------------------------------------------------
 * SlotContainerCmdResolver --
 *
 *    This is a specialized cmd resolver for slotcontainer.  The command
 *    resolver should be registered for a namespace and avoids the lookup of
 *    childobjs for unqualified calls. This way, it is e.g. possible to call
 *    in a slot-obj a method [list], even in cases, where a a property "list"
 *    is defined.
 *
 * Results:
 *    either TCL_CONTINUE or TCL_OK;
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int SlotContainerCmdResolver(Tcl_Interp *interp, const char *cmdName,
                         Tcl_Namespace *nsPtr, unsigned int flags, Tcl_Command *cmdPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(5);

static int
SlotContainerCmdResolver(Tcl_Interp *interp, const char *cmdName,
                         Tcl_Namespace *nsPtr, unsigned int flags, Tcl_Command *cmdPtr) {

  nonnull_assert(cmdName != NULL);
  nonnull_assert(nsPtr != NULL);
  nonnull_assert(cmdPtr != NULL);

  if (*cmdName == ':' || ((flags & TCL_GLOBAL_ONLY) != 0u)) {
    /* colon names (InterpColonCmdResolver) and global lookups are not for us */
    return TCL_CONTINUE;
  }

  /*fprintf(stderr, "SlotContainerCmdResolver called with %s ns %s ourNs %d clientData %p\n",
          cmdName, nsPtr->fullName, nsPtr->deleteProc == NSNamespaceDeleteProc,
          nsPtr->clientData);*/

  /*
   * Check, if this already a namespace handled by NSF
   */
  if (nsPtr->deleteProc == NSNamespaceDeleteProc && nsPtr->clientData) {
    NsfObject *parentObject = NSNamespaceClientDataObject(nsPtr->clientData);

    /*fprintf(stderr, "SlotContainerCmdResolver parentObject %p %s\n",
      parentObject, ObjectName(parentObject));*/
    /*
     * Make global lookups when the parent is a slotcontainer
     */
    /* parentObject = (NsfObject *) GetObjectFromString(interp, nsPtr->fullName);*/
    if ((parentObject->flags & NSF_IS_SLOT_CONTAINER) != 0u) {
      Tcl_Command cmd = Tcl_FindCommand(interp, cmdName, NULL, TCL_GLOBAL_ONLY);

      if (likely(cmd != NULL)) {
        *cmdPtr = cmd;
        return TCL_OK;
      }
    }
  }

  return TCL_CONTINUE;
 }

/*
 *----------------------------------------------------------------------
 * RequireObjNamespace --
 *
 *    Obtain for an object a namespace if necessary and initialize it.
 *    In this function, variables existing outside of the namespace
 *    get copied over to thew fresh namespace.
 *
 * Results:
 *    Tcl_Namespace
 *
 * Side effects:
 *    Allocate pot. a namespace
 *
 *----------------------------------------------------------------------
 */

static Tcl_Namespace *
RequireObjNamespace(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->nsPtr == NULL) {
    MakeObjNamespace(interp, object);
    NsfNamespaceInit(object->nsPtr);
  }
  assert(object->nsPtr != NULL);

  return object->nsPtr;
}

/*
 * Namespace related commands
 */
/*
 *----------------------------------------------------------------------
 * NSNamespacePreserve --
 *
 *    Increment namespace refCount
 *
 * Results:
 *    void
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void
NSNamespacePreserve(Tcl_Namespace *nsPtr) {

  nonnull_assert(nsPtr != NULL);

  MEM_COUNT_ALLOC("NSNamespace", nsPtr);
  Tcl_Namespace_refCount(nsPtr)++;
}
/*
 *----------------------------------------------------------------------
 * NSNamespaceRelease --
 *
 *    Decrement namespace refCount and free namespace if necessary
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Free pot. memory
 *
 *----------------------------------------------------------------------
 */
static void
NSNamespaceRelease(Tcl_Namespace *nsPtr) {

  nonnull_assert(nsPtr != NULL);

  MEM_COUNT_FREE("NSNamespace", nsPtr);
  Tcl_Namespace_refCount(nsPtr)--;
  if (unlikely(Tcl_Namespace_refCount(nsPtr) == 0 && (Tcl_Namespace_flags(nsPtr) & NS_DEAD))) {
    /*
     * The namespace refCount has reached 0, we have to free
     * it. unfortunately, NamespaceFree() is not exported
     */
    /*fprintf(stderr, "HAVE TO FREE namespace %p\n", nsPtr); */

    /*NamespaceFree(nsPtr);*/
    ckfree(nsPtr->fullName);
    ckfree(nsPtr->name);
    ckfree((char *)nsPtr);
  }
}

/*
 *----------------------------------------------------------------------
 * NSDeleteCmd --
 *
 *    Delete the Tcl command for the provided methodName located in
 *    the provided namespace.
 *
 * Results:
 *    Tcl result or -1, if no such method exists int.
 *
 * Side effects:
 *    Command is deleted.
 *
 *----------------------------------------------------------------------
 */
static int
NSDeleteCmd(Tcl_Interp *interp, Tcl_Namespace *nsPtr, const char *methodName) {
  Tcl_Command token;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);
  nonnull_assert(methodName != NULL);

  if ((token = FindMethod(nsPtr, methodName))) {
    return Tcl_DeleteCommandFromToken(interp, token);
  }
  return -1;
}


/*
 *----------------------------------------------------------------------
 * NSDeleteChild --
 *
 *    Delete a child of an object in cases, when the parent object is
 *    deleted. It is designed to delete either objects or classes to
 *    be a little bit more graceful on destuctors. Not perfect yet.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Might destroy an object.
 *
 *----------------------------------------------------------------------
 */
static int NSDeleteChild(Tcl_Interp *interp, Tcl_Command cmd, int deleteObjectsOnly) nonnull(1) nonnull(2);

static int
NSDeleteChild(Tcl_Interp *interp, Tcl_Command cmd, int deleteObjectsOnly) {

  nonnull_assert(cmd != NULL);
  nonnull_assert(interp != NULL);

  /*fprintf(stderr, "NSDeleteChildren child %p flags %.6x epoch %d\n",
    (void *)cmd, Tcl_Command_flags(cmd), Tcl_Command_cmdEpoch(cmd));*/

  /*
   * In some situations (e.g. small buckets, less than 12 entries), we
   * get from the cmd-table already deleted cmds; we had previously an
   * assert(Tcl_Command_cmdEpoch(cmd) == 0);
   * which will fail in such cases.
   */

  if (!Tcl_Command_cmdEpoch(cmd)) {
    NsfObject *object = NsfGetObjectFromCmdPtr(cmd);

    /*fprintf(stderr, "NSDeleteChildren child %p (%s) epoch %d\n",
      (void *)cmd, Tcl_GetCommandName(interp, cmd), Tcl_Command_cmdEpoch(cmd));*/

    if (object == NULL) {
      /*
       * This is just a plain Tcl command; let Tcl handle the
       * deletion.
       */
      return 0;
    }

    /*fprintf(stderr, "NSDeleteChild check %p %s true child %d\n",
      (void *)object, ObjectName(object), object->id == cmd);*/

    /* delete here just true children */
    if (object->id == cmd) {

      if (deleteObjectsOnly && NsfObjectIsClass(object)) {
        return 0;
      }
      /*fprintf(stderr, "NSDeleteChild destroy %p %s\n", (void *)object, ObjectName(object));*/

      /* in the exit handler physical destroy --> directly call destroy */
      if (RUNTIME_STATE(interp)->exitHandlerDestroyRound
          == NSF_EXITHANDLER_ON_PHYSICAL_DESTROY) {
        PrimitiveDestroy(object);
        return 1;
      } else {
        if (object->teardown && ((object->flags & NSF_DESTROY_CALLED) == 0u)) {
          int result;

          NsfObjectRefCountIncr(object);

          result = DispatchDestroyMethod(interp, object, 0);

          if (unlikely(result != TCL_OK)) {
            /*
             * The destroy method failed. However, we have to remove
             * the command anyway, since its parent is currently being
             * deleted.
             */
            /*fprintf(stderr, "==== NSDeleteChild DispatchDestroyMethod FAILED object %p (cmd %p) id %p teardown %p flags %.6x\n",
              (void *)object, (void *)cmd, (void *)object->id, (void *)object->teardown, object->flags);*/

            if (object->teardown != NULL) {
              NsfLog(interp, NSF_LOG_NOTICE, "Destroy failed for object %s %p %.6x, perform low level deletion",
                     (object->flags & NSF_DURING_DELETE) == NSF_DURING_DELETE ? "deleted-object" : ObjectName(object),
                     (void*)object, object->flags);
              CallStackDestroyObject(interp, object);
            }
          }
          NsfCleanupObject(object, "NSDeleteChild");

          return 1;
        }
      }
    } else {
      /*fprintf(stderr, "NSDeleteChild remove alias %p %s\n", (void*)object, Tcl_GetCommandName(interp, cmd));*/
      return AliasDeleteObjectReference(interp, cmd);
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * NSDeleteChildren --
 *
 *    Delete the child objects of a namespace.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Might destroy child objects.
 *
 *----------------------------------------------------------------------
 */

static void NSDeleteChildren(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1) nonnull(2);

static void
NSDeleteChildren(Tcl_Interp *interp, Tcl_Namespace *nsPtr) {
  Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(nsPtr);
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;
  int expected;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "NSDeleteChildren %p %s activationCount %d\n",
          nsPtr, nsPtr->fullName, Tcl_Namespace_activationCount(nsPtr));
#endif

  /*
   * First, get rid of namespace imported objects; don't delete the
   * object, but the reference.
   */
  Tcl_ForgetImport(interp, nsPtr, "*"); /* don't destroy namespace imported objects */


#if OBJDELETION_TRACE
  /*
   * Deletion is always tricky. Show, what elements should be deleted
   * in this loop. The actually deleted elements might be actually
   * less, if a deletion of one item triggers the destroy of another
   * item.
   */
  for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch); hPtr;
       hPtr = Tcl_NextHashEntry(&hSrch)) {
    Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
    fprintf(stderr, "will destroy %p %s\n", cmd, Tcl_GetCommandName(interp, cmd));
  }
#endif

  /*
   * Second, delete the objects.
   */

  /*
   * A destroy of one element of the hash-table can trigger the
   * destroy of another item of the same table. Therefore we use
   * Nsf_NextHashEntry(), which handles this case.
   */
  for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch); hPtr;
       hPtr = Nsf_NextHashEntry(cmdTablePtr, expected, &hSrch)) {
    /*Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
    fprintf(stderr, "NSDeleteChild %p table %p numEntries before %d\n",
    cmd, hPtr->tablePtr, cmdTablePtr->numEntries );*/
    expected = cmdTablePtr->numEntries -
      NSDeleteChild(interp, (Tcl_Command)Tcl_GetHashValue(hPtr), 1);
  }
 /*
  * Finally, delete the classes.
   */
  for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch); hPtr;
       hPtr = Nsf_NextHashEntry(cmdTablePtr, expected, &hSrch)) {
    expected = cmdTablePtr->numEntries -
      NSDeleteChild(interp, (Tcl_Command)Tcl_GetHashValue(hPtr), 0);
  }
}

/*
 * delete all vars & procs in a namespace
 */
static void NSCleanupNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1) nonnull(2);

static void
NSCleanupNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) {
  TclVarHashTable *varTablePtr = Tcl_Namespace_varTablePtr(nsPtr);
  Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(nsPtr);
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "NSCleanupNamespace %p flags %.6x\n", nsPtr, Tcl_Namespace_flags(nsPtr));
  fprintf(stderr, "NSCleanupNamespace %p %.6x varTablePtr %p\n", nsPtr, ((Namespace *)nsPtr)->flags, varTablePtr);
#endif
  /*
   * Delete all variables and initialize var table again
   * (DeleteVars frees the var-table)
   */
  TclDeleteVars((Interp *)interp, varTablePtr);
  TclInitVarHashTable(varTablePtr, (Namespace *)nsPtr);

  /*
   * Delete all user-defined procs in the namespace
   */
  for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch); hPtr;
       hPtr = Tcl_NextHashEntry(&hSrch)) {
      Tcl_Command cmd = (Tcl_Command) Tcl_GetHashValue(hPtr);

      if (CmdIsNsfObject(cmd)) {
        /*
         * Sub-objects should not be deleted here to preserve children
         * deletion order. Just delete aliases.
         */
        AliasDeleteObjectReference(interp, cmd);
        continue;
      }
      /*fprintf(stderr, "NSCleanupNamespace calls DeleteCommandFromToken for %p flags %.6x\n",
                cmd, ((Command *)cmd)->flags);
      fprintf(stderr, "    cmd = %s\n", Tcl_GetCommandName(interp, cmd));
      fprintf(stderr, "    nsPtr = %p\n", ((Command *)cmd)->nsPtr);
      fprintf(stderr, "    epoch = %d\n", Tcl_Command_cmdEpoch(cmd));
      fprintf(stderr, "    refCount = %d\n", Tcl_Command_refCount(cmd));
      fprintf(stderr, "    flags %.6x\n", ((Namespace *)((Command *)cmd)->nsPtr)->flags);*/

      Tcl_DeleteCommandFromToken(interp, cmd);
  }
}


static void
NSNamespaceDeleteProc(ClientData clientData) {
  NsfObject *object;

  nonnull_assert(clientData != NULL);

  object = NSNamespaceClientDataObject(clientData);
  assert(object != NULL);

#ifdef NSF_MEM_COUNT
  ckfree((char *)clientData);
#endif

  /*fprintf(stderr, "namespace delete-proc obj=%p ns=%p\n",
    clientData, (object != NULL) ? object->nsPtr : NULL);*/

  MEM_COUNT_FREE("NSNamespace", object->nsPtr);
  object->nsPtr = NULL;
}

void Nsf_DeleteNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1) nonnull(2);

void
Nsf_DeleteNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) {
  int activationCount = 0;
  Tcl_CallFrame *f = (Tcl_CallFrame *)Tcl_Interp_framePtr(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);

  /*fprintf(stderr, "Nsf_DeleteNamespace %p ", nsPtr);*/

  while (f) {
    if (f->nsPtr == nsPtr) {
      activationCount++;
    }
    f = Tcl_CallFrame_callerPtr(f);
  }

#if !defined(NDEBUG)
  if (Tcl_Namespace_activationCount(nsPtr) != activationCount) {
    fprintf(stderr, "WE HAVE TO FIX ACTIVATIONCOUNT\n");
    Tcl_Namespace_activationCount(nsPtr) = activationCount;
  }
#endif
  assert(Tcl_Namespace_activationCount(nsPtr) == activationCount);

  /*fprintf(stderr, "to %d. \n", ((Namespace *)nsPtr)->activationCount);*/

  if (Tcl_Namespace_deleteProc(nsPtr)) {
    /*fprintf(stderr, "calling deteteNamespace %s\n", nsPtr->fullName);*/
    Tcl_DeleteNamespace(nsPtr);
  }
}

/*
 *----------------------------------------------------------------------
 * NSValidObjectName --
 *
 *    Check the provided colons in an object name. If the name is
 *    valid, the function returns 1, otherwise 0.
 *
 * Results:
 *    returns 1 on success
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int NSValidObjectName(const char *name, size_t l) nonnull(1);

NSF_INLINE static int
NSValidObjectName(const char *name, size_t l) {
  register const char *n;

  nonnull_assert(name != NULL);

  n = name;
  if (*n == '\0') {
    return 0; /* empty name */
  }
  if (l == 0) {
    l = strlen(n);
  }
  if (*(n+l-1) == ':') {
    return 0; /* name ends with : */
  }
  if (*n == ':' && *(n+1) != ':') {
    return 0; /* name begins with single : */
  }
  for (; *n != '\0'; n++) {
    if (*n == ':' && *(n+1) == ':' && *(n+2) == ':') {
      return 0;   /* more than 2 colons in series in a name */
    }
  }
  return 1;
}

/*
 *----------------------------------------------------------------------
 * NSGetFreshNamespace --
 *
 *    Create an object namespace, provide a deleteProc (avoid
 *    interference between object and namespace deletion order) and
 *    keep the object as client data.
 *
 * Results:
 *    Tcl_Namespace
 *
 * Side effects:
 *    might allocate a namespace
 *
 *----------------------------------------------------------------------
 */
static Tcl_Namespace*
NSGetFreshNamespace(Tcl_Interp *interp, NsfObject *object, const char *name) {
  Namespace *dummy1Ptr, *dummy2Ptr, *nsPtr;
  const char *dummy;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(name != NULL);

  TclGetNamespaceForQualName(interp, name, NULL, TCL_FIND_ONLY_NS|TCL_CREATE_NS_IF_UNKNOWN,
                             &nsPtr, &dummy1Ptr, &dummy2Ptr, &dummy);

  if (nsPtr->deleteProc != NSNamespaceDeleteProc) {
    /*
     * Avoid hijacking a namespace with different client data
     */
    if (nsPtr->deleteProc || nsPtr->clientData) {
      Tcl_Panic("Namespace '%s' exists already with delProc %p and clientData %p; "
                "Can only convert a plain Tcl namespace into an nsf namespace, my delete Proc %p",
                name, nsPtr->deleteProc, nsPtr->clientData, NSNamespaceDeleteProc);
    }

    {
#ifdef NSF_MEM_COUNT
      NsfNamespaceClientData *nsClientData = (NsfNamespaceClientData *)ckalloc(sizeof(NsfNamespaceClientData));

      nsClientData->object = object;
      nsClientData->nsPtr = (Tcl_Namespace *)nsPtr;
      nsPtr->clientData = nsClientData;

      /*fprintf(stderr, "Adding NsfNamespaceClientData nsPtr %p cd %p name '%s'\n",
        nsPtr, nsClientData, nsPtr->fullName);*/
#else
      nsPtr->clientData = object;
#endif
      nsPtr->deleteProc = (Tcl_NamespaceDeleteProc *)NSNamespaceDeleteProc;
    }
    MEM_COUNT_ALLOC("NSNamespace", nsPtr);
  } else {
    fprintf(stderr, "NSGetFreshNamespace: reusing namespace %p %s\n", (void *)nsPtr, nsPtr->fullName);
  }

  return (Tcl_Namespace *)nsPtr;
}

/*
 *----------------------------------------------------------------------
 * NSRequireParentObject --
 *
 *    Try to require a parent object (e.g. during ttrace).  This function
 *    tries to load a parent object via ::nsf::object::unknown.
 *
 * Results:
 *    returns 1 on success
 *
 * Side effects:
 *    might create an object
 *
 *----------------------------------------------------------------------
 */
static int NSRequireParentObject(Tcl_Interp *interp, const char *parentName) nonnull(1) nonnull(2);

static int
NSRequireParentObject(Tcl_Interp *interp, const char *parentName) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(parentName != NULL);

  result = NsfCallObjectUnknownHandler(interp, Tcl_NewStringObj(parentName, -1));

  if (likely(result == TCL_OK)) {
    NsfObject *parentObj = (NsfObject *) GetObjectFromString(interp, parentName);
    if (parentObj != NULL) {
      RequireObjNamespace(interp, parentObj);
    }
    result = (Tcl_FindNamespace(interp, parentName,
                            (Tcl_Namespace *) NULL, TCL_GLOBAL_ONLY) != NULL);
  }
  return result;

}

/*
 *----------------------------------------------------------------------
 * NSCheckNamespace --
 *
 *    Check if a namespace with the given name exists. If not, make
 *    sure that a potential parent object has already required a
 *    namespace. If there is no parent namespace yet, try to create a
 *    parent object via __unknown.

 *    If the provided parentNsPtr is not NULL, we know, that (a) the
 *    provided name was relative and simple (contains no ":"
 *    characters) and that (b) this namespace was used to build a fully
 *    qualified name. In theses cases, the parentNsPtr points already
 *    to the parentName, containing potentially a parent Object. In
 *    all other cases, the parent name is either obtained from the
 *    full namespace, or from string operations working on the
 *    provided name.
 *
 * Results:
 *    Tcl_Namespace for the provided name
 *
 * Side effects:
 *    might create parent objects
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static Tcl_Namespace *NSCheckNamespace(Tcl_Interp *interp, const char *nameString, Tcl_Namespace *parentNsPtr1)
  nonnull(1) nonnull(2);

NSF_INLINE static Tcl_Namespace *
NSCheckNamespace(Tcl_Interp *interp, const char *nameString, Tcl_Namespace *parentNsPtr1) {
  Namespace *nsPtr, *dummy1Ptr, *dummy2Ptr, *parentNsPtr = (Namespace *)parentNsPtr1;
  const char *parentName, *dummy, *n;
  Tcl_DString ds, *dsPtr = &ds;
  int parentNameLength;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameString != NULL);

  /*fprintf(stderr, "NSCheckNamespace %s parentNsPtr %p\n", nameString, parentNsPtr);*/

  /*
   * Check, if there is a already a namespace for the full name. The
   * namespace will be seldomly here, but we have to make this check
   * in every case. If there is a full namespace, we can use it to
   * determine the parent name.
   */
  TclGetNamespaceForQualName(interp, nameString, NULL,
                             TCL_GLOBAL_ONLY|TCL_FIND_ONLY_NS,
                             &nsPtr, &dummy1Ptr, &dummy2Ptr, &dummy);
  /*fprintf(stderr,
          "before create calls TclGetNamespaceForQualName with %s => %p (%s) %p %s %p %s %p %s\n",
          nameString, nsPtr, (nsPtr != NULL) ? nsPtr->fullName : "",
          dummy1Ptr, (dummy1Ptr != NULL) ? dummy1Ptr->fullName : "",
          parentNsPtr, (parentNsPtr != NULL) ? parentNsPtr->fullName : "",
          dummy, (dummy != NULL) ? dummy : "");*/

  /*
   * If there is a parentNs provided (or obtained from the full
   * namespace), we can determine the parent name from it. Otherwise,
   * we have to to perform the string operations.
   */

  if (parentNsPtr == NULL && nsPtr) {
    parentNsPtr = Tcl_Namespace_parentPtr(nsPtr);
  }

  if (parentNsPtr != NULL) {
    parentNameLength = 0;
    parentName = parentNsPtr->fullName;
    if (*(parentName + 2) == '\0') {
      parentName = NULL;
    }
    /*fprintf(stderr, "NSCheckNamespace parentNs %s parentName of '%s' => '%s'\n",
      parentNsPtr->fullName, nameString, parentName);*/
  } else {
    n = nameString + strlen(nameString);
    /*
     * search for last '::'
     */
    while ((*n != ':' || *(n-1) != ':') && n-1 > nameString) {n--; }
    if (*n == ':' && n > nameString && *(n-1) == ':') {n--;}
    parentNameLength = n-nameString;
    if (parentNameLength > 0) {
      DSTRING_INIT(dsPtr);
      Tcl_DStringAppend(dsPtr, nameString, parentNameLength);
      parentName = Tcl_DStringValue(dsPtr);
    } else {
      parentName = NULL;
    }
  }

  if (parentName != NULL) {
    NsfObject *parentObj;
    parentObj = (NsfObject *) GetObjectFromString(interp, parentName);
    /*fprintf(stderr, "parentName %s parentObj %p\n", parentName, parentObj);*/

    if (parentObj != NULL) {
      RequireObjNamespace(interp, parentObj);
    } else if (nsPtr == NULL && parentNsPtr == NULL) {
      TclGetNamespaceForQualName(interp, parentName, NULL,
                                 TCL_GLOBAL_ONLY|TCL_FIND_ONLY_NS,
                                 &parentNsPtr, &dummy1Ptr,
                                 &dummy2Ptr, &dummy);
      if (parentNsPtr == NULL) {
        /*fprintf(stderr, "===== calling NSRequireParentObject %s %p\n", parentName, cl);*/
        NSRequireParentObject(interp, parentName);
      }
    }

    if (parentNameLength > 0) {
      DSTRING_FREE(dsPtr);
    }
  }

  return (Tcl_Namespace *)nsPtr;
}


/*
 *----------------------------------------------------------------------
 * NSFindCommand --
 *
 *    Find the "real" command belonging eg. to an Next Scripting class or object.
 *    Do not return cmds produced by Tcl_Import, but the "real" cmd
 *    to which they point.
 *
 * Results:
 *    Tcl_Command or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

NSF_INLINE static Tcl_Command NSFindCommand(Tcl_Interp *interp, const char *name) nonnull(1) nonnull(2);

NSF_INLINE static Tcl_Command
NSFindCommand(Tcl_Interp *interp, const char *name) {
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  assert(*name == ':' && *(name + 1) == ':');

  cmd = Tcl_FindCommand(interp, name, NULL, TCL_GLOBAL_ONLY);
  if (likely(cmd != NULL)) {
    Tcl_Command importedCmd = TclGetOriginalCommand(cmd);
    if (unlikely(importedCmd != NULL)) {
      cmd = importedCmd;
    }
  }
  return cmd;
}

#if !defined(NDEBUG)
/*
 *----------------------------------------------------------------------
 * ReverseLookupCmdFromCmdTable --
 *
 *    Allows for looking up objects in command tables (e.g., namespace cmd
 *    tables, the interp's hidden cmd table) based on their command pointer
 *    (rather than their command name).
 *
 * Results:
 *    NsfObject* or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int ReverseLookupCmdFromCmdTable(Tcl_Interp *interp /* needed? */, Tcl_Command searchCmdPtr,
                             Tcl_HashTable *cmdTablePtr) nonnull(1) nonnull(3);

static int
ReverseLookupCmdFromCmdTable(Tcl_Interp *interp /* needed? */, Tcl_Command searchCmdPtr,
                             Tcl_HashTable *cmdTablePtr) {
  Tcl_HashSearch search;
  Tcl_HashEntry *hPtr;

  nonnull_assert(searchCmdPtr != NULL);
  nonnull_assert(cmdTablePtr != NULL);

  for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &search);
       hPtr != NULL;
       hPtr = Tcl_NextHashEntry(&search)) {
    Tcl_Command needleCmdPtr = (Tcl_Command)Tcl_GetHashValue(hPtr);

    if (needleCmdPtr == searchCmdPtr) {
      return 1;
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetHiddenObjectFromCmd --
 *
 *    Obtains a hidden object for a specified cmd. The function uses a reverse
 *    lookup of *hidden* object structures based on their commands. This
 *    helper is needed for handling hidden and re-exposed objects during the
 *    shutdown and the cleanup of object systems.
 *
 * Results:
 *    NsfObject* or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static NsfObject * GetHiddenObjectFromCmd(Tcl_Interp *interp, Tcl_Command cmdPtr) nonnull(1);

static NsfObject *
GetHiddenObjectFromCmd(Tcl_Interp *interp, Tcl_Command cmdPtr) {
  Interp *iPtr = (Interp *) interp;
  NsfObject *screenedObject;
  int found;

  nonnull_assert(cmdPtr != NULL);

  /*
   * We can provide a shortcut, knowing that a) exposed cmds have an epoch
   * counter > 0, and b) the commands originating namespace must be the global
   * one. See also Tcl_HideCommand() and Tcl_ExposeCommand().
   */
  if (Tcl_Command_cmdEpoch(cmdPtr) == 0 ||
      ((Command *)cmdPtr)->nsPtr != iPtr->globalNsPtr) {
    return NULL;
  }

  /*
   * Reverse lookup object in the interp's hidden command table. We start
   * off with the hidden cmds as we suspect their number being smaller than
   * the re-exposed ones, living in the global namespace
   */
  found = ReverseLookupCmdFromCmdTable(interp, cmdPtr, iPtr->hiddenCmdTablePtr);
  if (found == 0) {
    /*
     * Reverse lookup object in the interp's global command table. Most likely
     * needed due to hiding + exposing on a different name.
     */
    found = ReverseLookupCmdFromCmdTable(interp, cmdPtr, &iPtr->globalNsPtr->cmdTable);
  }
  screenedObject = (found != 0) ? NsfGetObjectFromCmdPtr(cmdPtr) : NULL;

#if !defined(NDEBUG)
  if (screenedObject != NULL) {
    NsfLog(interp, NSF_LOG_NOTICE, "screened object %s found: object %p (%s) cmd %p",
           Tcl_GetCommandName(interp, cmdPtr), (void *)screenedObject,
           ObjectName(screenedObject), (void *)cmdPtr);
  }
#endif
  return screenedObject;
}
#endif

/*
 *----------------------------------------------------------------------
 * GetObjectFromString --
 *
 *    Lookup an object from a given string. The function performs a
 *    command lookup (every object is a command) and checks, if the
 *    command is bound to an nsf object.
 *
 * Results:
 *    NsfObject* or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfObject *
GetObjectFromString(Tcl_Interp *interp, const char *name) {
  register Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);

  /*fprintf(stderr, "GetObjectFromString name = '%s'\n", name);*/
  cmd = NSFindCommand(interp, name);

  if (likely(cmd && CmdIsNsfObject(cmd))) {
    /*fprintf(stderr, "GetObjectFromString %s => %p\n", name, Tcl_Command_objClientData(cmd));*/
    return (NsfObject *)Tcl_Command_objClientData(cmd);
  }
  /*fprintf(stderr, "GetObjectFromString %s => NULL\n", name);*/
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * GetClassFromString --
 *
 *    Lookup a class from a given string. The function performs an
 *    object lookup and checks, if the object is a class
 *
 * Results:
 *    NsfClass* or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfClass *
GetClassFromString(Tcl_Interp *interp, const char *name) {
  NsfObject *object = GetObjectFromString(interp, name);

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  return (object != NULL && NsfObjectIsClass(object)) ? (NsfClass *)object : NULL;
}

/*
 *----------------------------------------------------------------------
 * CanRedefineCmd --
 *
 *    This function tests, whether a method (provided as a string) is
 *    allowed to be redefined in a provided namespace.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int CanRedefineCmd(Tcl_Interp *interp, Tcl_Namespace *nsPtr, NsfObject *object, const char *methodName, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
CanRedefineCmd(Tcl_Interp *interp, Tcl_Namespace *nsPtr, NsfObject *object, const char *methodName, unsigned int flags) {
  int result, ok;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);

  cmd = FindMethod(nsPtr, methodName);

  if (cmd != NULL) {
    if ( NsfGetObjectFromCmdPtr(cmd) != NULL) {
      /*
       * Don't allow overwriting of an object with an method.
       */
      return NsfPrintError(interp,
                           "refuse to overwrite child object with method %s; delete/rename it before overwriting",
                           methodName);
    }
    ok = (Tcl_Command_flags(cmd) & NSF_CMD_REDEFINE_PROTECTED_METHOD) == 0;
  } else {
    ok = 1;
  }

  if (likely(ok != 0)) {
    result = TCL_OK;
  } else {
    /*
     * We could test, whether we are bootstrapping the "right" object
     * system, and allow only overwrites for the current bootstrap
     * object system, but this seems necessary by now.
     */
    Tcl_Obj *bootstrapObj = Tcl_GetVar2Ex(interp, "::nsf::bootstrap", NULL, TCL_GLOBAL_ONLY);
    if (bootstrapObj == NULL) {
      result = NsfPrintError(interp, "refuse to overwrite protected method '%s'; "
                             "derive e.g. a sub-class!", methodName, ObjectName(object));
    } else {
      result = TCL_OK;
    }
  }

  if (likely(result == TCL_OK)) {
    result = ObjectSystemsCheckSystemMethod(interp, methodName, object, flags);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfAddObjectMethod --
 *
 *    Externally callable function to register an object level method
 *    for the provided object.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Newly created Tcl command.
 *
 *----------------------------------------------------------------------
 */
int NsfAddObjectMethod(Tcl_Interp *interp, Nsf_Object *object1, const char *methodName,
                       Tcl_ObjCmdProc *proc, ClientData clientData, Tcl_CmdDeleteProc *dp,
                       unsigned int flags) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

int
NsfAddObjectMethod(Tcl_Interp *interp, Nsf_Object *object1, const char *methodName,
                   Tcl_ObjCmdProc *proc, ClientData clientData, Tcl_CmdDeleteProc *dp,
                   unsigned int flags) {
  NsfObject *object;
  Tcl_DString newCmdName, *dsPtr = &newCmdName;
  Tcl_Namespace *ns;
  Tcl_Command newCmd;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object1 != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(proc != NULL);

  object = (NsfObject *)object1;
  ns = RequireObjNamespace(interp, object);

  /* Check, if we are allowed to redefine the method */
  result = CanRedefineCmd(interp, object->nsPtr, object, (char *)methodName, flags);
  if (unlikely(result != TCL_OK)) {
    return result;
  }
  NsfObjectMethodEpochIncr("NsfAddObjectMethod");

  /* delete an alias definition, if it exists */
  AliasDelete(interp, object->cmdName, methodName, 1);

  Tcl_DStringInit(dsPtr);
  DStringAppendQualName(dsPtr, ns, methodName);

  newCmd = Tcl_CreateObjCommand(interp, Tcl_DStringValue(dsPtr), proc, clientData, dp);

  if (flags != 0) {
    ((Command *) newCmd)->flags |= flags;
  }
  Tcl_DStringFree(dsPtr);
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NsfAddClassMethod --
 *
 *    Externally callable function to register an class level method
 *    for the provided class.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Newly created Tcl command.
 *
 *----------------------------------------------------------------------
 */
int NsfAddClassMethod(Tcl_Interp *interp, Nsf_Class *class, const char *methodName,
                      Tcl_ObjCmdProc *proc, ClientData clientData, Tcl_CmdDeleteProc *dp,
                      unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

int
NsfAddClassMethod(Tcl_Interp *interp, Nsf_Class *class, const char *methodName,
                  Tcl_ObjCmdProc *proc, ClientData clientData, Tcl_CmdDeleteProc *dp,
                  unsigned int flags) {
  Tcl_DString newCmdName, *dsPtr = &newCmdName;
  Tcl_Command newCmd;
  NsfClass *cl;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(proc != NULL);

  cl = (NsfClass *)class;
  assert(cl->nsPtr != NULL);

   /* Check, if we are allowed to redefine the method */
  result = CanRedefineCmd(interp, cl->nsPtr, &cl->object, (char *)methodName, flags);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  NsfInstanceMethodEpochIncr("NsfAddClassMethod");

 /* delete an alias definition, if it exists */
  AliasDelete(interp, class->object.cmdName, methodName, 0);

  Tcl_DStringInit(dsPtr);
  DStringAppendQualName(dsPtr, cl->nsPtr, methodName);

  newCmd = Tcl_CreateObjCommand(interp, Tcl_DStringValue(dsPtr), proc, clientData, dp);

  if (flags != 0) {
    ((Command *) newCmd)->flags |= flags;
  }
  Tcl_DStringFree(dsPtr);
  return TCL_OK;
}

/*
 * Auto-naming
 */

static Tcl_Obj * AutonameIncr(Tcl_Interp *interp, Tcl_Obj *nameObj, NsfObject *object,
             int instanceOpt, int resetOpt) nonnull(1) nonnull(2) nonnull(3);

static Tcl_Obj *
AutonameIncr(Tcl_Interp *interp, Tcl_Obj *nameObj, NsfObject *object,
             int isInstanceOpt, int doResetOpt) {
  Tcl_Obj *valueObj, *resultObj = NULL;
  CallFrame frame, *framePtr = &frame;
  int flogs = TCL_LEAVE_ERR_MSG;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(object != NULL);

  Nsf_PushFrameObj(interp, object, framePtr);
  if (object->nsPtr != NULL) {
    flogs |= TCL_NAMESPACE_ONLY;
  }
  valueObj = Tcl_ObjGetVar2(interp, NsfGlobalObjs[NSF_AUTONAMES], nameObj, flogs);
  if (valueObj != NULL) {
    long autoname_counter;

    /* should probably do an overflow check here */
    Tcl_GetLongFromObj(interp, valueObj, &autoname_counter);
    autoname_counter++;
    if (Tcl_IsShared(valueObj)) {
      valueObj = Tcl_DuplicateObj(valueObj);
    }
    Tcl_SetLongObj(valueObj, autoname_counter);
    resultObj = Tcl_ObjSetVar2(interp, NsfGlobalObjs[NSF_AUTONAMES], nameObj,
                               valueObj, flogs);
    if (unlikely(resultObj == NULL)) {
      return NULL;
    }
  }

  if (doResetOpt == 1) {
    if (valueObj != NULL) { /* we have an entry */
      Tcl_UnsetVar2(interp, NsfGlobalStrings[NSF_AUTONAMES], ObjStr(nameObj), flogs);
    }
    resultObj = NsfGlobalObjs[NSF_EMPTY];
    INCR_REF_COUNT2("autoname", resultObj);
  } else {
    int mustCopy = 1, format = 0;
    const char *c;

    if (valueObj == NULL) {
      valueObj = Tcl_ObjSetVar2(interp, NsfGlobalObjs[NSF_AUTONAMES],
                                   nameObj, NsfGlobalObjs[NSF_ONE], flogs);
    }
    if (isInstanceOpt == 1) {
      char firstChar;
      const char *nextChars = ObjStr(nameObj);

      firstChar = *(nextChars ++);
      if (isupper((int)firstChar)) {
        char buffer[1];
        buffer[0] = tolower((int)firstChar);
        resultObj = Tcl_NewStringObj(buffer, 1);
        INCR_REF_COUNT2("autoname", resultObj);
        Tcl_AppendLimitedToObj(resultObj, nextChars, -1, INT_MAX, NULL);
        mustCopy = 0;
      }
    }
    if (mustCopy == 1) {
      resultObj = Tcl_DuplicateObj(nameObj);
      INCR_REF_COUNT2("autoname", resultObj);
      /*
        fprintf(stderr, "*** copy %p %s = %p\n", name, ObjStr(name), resultObj);
      */
    }
    /* if we find a % in the autoname -> We use Tcl_FormatObjCmd
       to let the autoname string be formated, like Tcl "format"
       command, with the value. E.g.:
       autoname a%06d --> a000000, a000001, a000002, ...
    */
    for (c = ObjStr(resultObj); *c != '\0'; c++) {
      if (*c == '%') {
        if (*(c+1) != '%') {
          format = 1;
          break;
        } else {
          /* when we find a %% we format and then append autoname, e.g.
             autoname a%% --> a%1, a%2, ... */
          c++;
        }
      }
    }
    if (format == 1) {
      Tcl_Obj *savedResultObj;

      ALLOC_ON_STACK(Tcl_Obj*, 3, ov);
      savedResultObj = Tcl_GetObjResult(interp);
      INCR_REF_COUNT(savedResultObj);
      ov[1] = resultObj;
      ov[2] = valueObj;
      if (NsfCallCommand(interp, NSF_FORMAT, 3, ov) != TCL_OK) {
        Nsf_PopFrameObj(interp, framePtr);
        DECR_REF_COUNT(savedResultObj);
        FREE_ON_STACK(Tcl_Obj*, ov);
        return NULL;
      }
      DECR_REF_COUNT(resultObj);
      resultObj = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
      INCR_REF_COUNT2("autoname", resultObj);
      Tcl_SetObjResult(interp, savedResultObj);
      DECR_REF_COUNT(savedResultObj);
      FREE_ON_STACK(Tcl_Obj*, ov);

    } else {
      const char *valueString = Tcl_GetString(valueObj);

      Tcl_AppendLimitedToObj(resultObj, valueString, valueObj->length, INT_MAX, NULL);
      /*fprintf(stderr, "+++ append to obj done\n");*/
    }
  }

  Nsf_PopFrameObj(interp, framePtr);
  assert((doResetOpt == 1 && resultObj->refCount>=1) || (resultObj->refCount == 1));
  return resultObj;
}

/*
 * Next Scripting CallStack functions
 */

NSF_INLINE static void CallStackDoDestroy(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

NSF_INLINE static void
CallStackDoDestroy(Tcl_Interp *interp, NsfObject *object) {
  Tcl_Command oid;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*fprintf(stderr, "CallStackDoDestroy %p flags %.6x\n", object, object->flags);*/
  PRINTOBJ("CallStackDoDestroy", object);

  /*
   * Don't do anything, if a recursive DURING_DELETE is for some
   * reason active.
   */
  if ((object->flags & NSF_DURING_DELETE) != 0u) {
    return;
  }
  /*fprintf(stderr, "CallStackDoDestroy %p flags %.6x activation %d object->refCount %d cmd %p \n",
    object, object->flags, object->activationCount, object->refCount, object->id);*/

  object->flags |= NSF_DURING_DELETE;
  oid = object->id;
  /* oid might be freed already, we can't even use (((Command *)oid)->flags & CMD_IS_DELETED) */

  if (object->teardown && oid) {
    /*
     * PrimitiveDestroy() has to be before DeleteCommandFromToken(),
     * otherwise e.g. unset traces on this object cannot be executed
     * from Tcl. We make sure via refCounting that the object
     * structure is kept until after DeleteCommandFromToken().
     */
    NsfObjectRefCountIncr(object);

    PrimitiveDestroy(object);

    if /*(object->teardown == NULL)*/ ((object->flags & NSF_TCL_DELETE) == 0u) {
      Tcl_Obj *savedResultObj = Tcl_GetObjResult(interp);
      INCR_REF_COUNT(savedResultObj);

      assert(object->teardown == NULL);

      /*fprintf(stderr, "    before DeleteCommandFromToken %p object flags %.6x\n", (void *)oid, object->flags);*/
      /*fprintf(stderr, "cmd dealloc %p refCount %d dodestroy \n", (void *)oid, Tcl_Command_refCount(oid));*/
      Tcl_DeleteCommandFromToken(interp, oid); /* this can change the result */
      /*fprintf(stderr, "    after DeleteCommandFromToken %p %.6x\n", (void *)oid, ((Command *)oid)->flags);*/
      Tcl_SetObjResult(interp, savedResultObj);
      DECR_REF_COUNT(savedResultObj);
    }

    NsfCleanupObject(object, "CallStackDoDestroy");
  }
}

static void
CallStackDestroyObject(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "CallStackDestroyObject %p %s activationcount %d flags %.6x\n",
          object, ObjectName(object), object->activationCount, object->flags);
#endif

  if ((object->flags & NSF_DESTROY_CALLED) == 0u) {
    int activationCount = object->activationCount;

    /* if the destroy method was not called yet, do it now */
#ifdef OBJDELETION_TRACE
    fprintf(stderr, "  CallStackDestroyObject has to DispatchDestroyMethod %p activationCount %d\n",
            object, activationCount);
#endif
    DispatchDestroyMethod(interp, object, 0);

    if (activationCount == 0) {
      /*
       * We assume, the object is now freed. If the object is already
       * freed, we cannot access activation count, and we cannot call
       * CallStackDoDestroy.
       */
      /*fprintf(stderr, "  CallStackDestroyObject %p done\n",  obj);*/
      return;
    }
  }

  /*
   * If the object is not referenced on the call-stack anymore
   * we have to destroy it directly, because CscFinish won't
   * find the object destroy.
   */
  if (object->activationCount == 0) {
    CallStackDoDestroy(interp, object);
  } else {
    /*
     * To prevail the deletion order call delete children now -> children
     * destructors are called before parent's destructor.
     */
    if (object->teardown && object->nsPtr) {
      /*fprintf(stderr, "  CallStackDestroyObject calls NSDeleteChildren\n");*/
      NSDeleteChildren(interp, object->nsPtr);
    }
  }
  /*fprintf(stderr, "  CallStackDestroyObject %p DONE\n",  object);*/
}

/*
 * cmd list handling
 */

/*
 *----------------------------------------------------------------------
 * CmdListAdd --
 *
 *    Add an entry to a cmdlist. Optionally, the function checks for
 *    duplicates (does not insert a duplicate) or it allows to add to the end
 *    of the list.
 *
 * Results:
 *    The newly inserted command list item or a found item (never null)
 *
 * Side effects:
 *    Added List entry.
 *
 *----------------------------------------------------------------------
 */
static NsfCmdList *CmdListAdd(NsfCmdList **cList, Tcl_Command c, NsfClass *clorobj, int noDuplicates, int atEnd)
  nonnull(1) nonnull(2) returns_nonnull;

static NsfCmdList *
CmdListAdd(NsfCmdList **cList, Tcl_Command cmd, NsfClass *clorobj, int noDuplicates, int atEnd) {
  NsfCmdList *l, *nextPtr, *new;

  nonnull_assert(cmd != NULL);
  nonnull_assert(cList != NULL);

  if (unlikely(atEnd)) {
    l = *cList;
    nextPtr = NULL;
  } else {
    l = NULL;
    nextPtr = *cList;
  }

  /*
   * check for duplicates, if necessary
   */
  if (unlikely(noDuplicates)) {
    NsfCmdList *h = l, **end = NULL;
    while (h) {
      if (h->cmdPtr == cmd) {
        return h;
      }
      end = &(h->nextPtr);
      h = h->nextPtr;
    }
    if (end != NULL) {
      /* no duplicates, no need to search below, we are at the end of the list */
      cList = end;
      l = NULL;
    }
  }

  /*
   * ok, we have no duplicates -> append "new"
   * to the end of the list
   */
  new = NEW(NsfCmdList);
  new->cmdPtr = cmd;
  NsfCommandPreserve(new->cmdPtr);
  new->clientData = NULL;
  new->clorobj = clorobj;
  new->nextPtr = nextPtr;

  if (unlikely(l != NULL)) {
    /*
     * append new element at the end
     */
    while (l->nextPtr) {
      l = l->nextPtr;
    }
    l->nextPtr = new;
  } else {
    /*
     * prepend new element
     */
    *cList = new;
  }

  return new;
}

/*
 *----------------------------------------------------------------------
 * CmdListAddSorted --
 *
 *    Add an entry to a cmdlist without duplicates. The order of the entries
 *    is not supposed to be relevant. This function maintains a sorted list to
 *    reduce cost to n/2. Can be improved be using better data structures of
 *    needed.
 *
 * Results:
 *    The newly inserted command list item or a found item
 *
 * Side effects:
 *    Added List entry.
 *
 *----------------------------------------------------------------------
 */
static NsfCmdList *CmdListAddSorted(NsfCmdList **cList, Tcl_Command c, NsfClass *clorobj)
  nonnull(1) nonnull(2) returns_nonnull;

static NsfCmdList *
CmdListAddSorted(NsfCmdList **cList, Tcl_Command cmd, NsfClass *clorobj) {
  NsfCmdList *prev, *new, *h;

  nonnull_assert(cmd != NULL);
  nonnull_assert(cList != NULL);

  for (h = *cList, prev = NULL; h; prev = h, h = h->nextPtr) {
    if (h->cmdPtr == cmd) {
      return h;
    } else if (h->cmdPtr > cmd) {
      break;
    }
  }

  new = NEW(NsfCmdList);
  new->cmdPtr = cmd;
  NsfCommandPreserve(new->cmdPtr);
  new->clientData = NULL;
  new->clorobj = clorobj;
  new->nextPtr = h;

  if (prev != NULL) {
    prev->nextPtr = new;
  } else {
    *cList = new;
  }

  return new;
}

static void CmdListReplaceCmd(NsfCmdList *replace, Tcl_Command cmd, NsfClass *clorobj)
  nonnull(1) nonnull(3);

static void
CmdListReplaceCmd(NsfCmdList *replace, Tcl_Command cmd, NsfClass *clorobj) {
  Tcl_Command del;

  nonnull_assert(replace != NULL);
  nonnull_assert(clorobj != NULL);

  del = replace->cmdPtr;
  replace->cmdPtr = cmd;
  replace->clorobj = clorobj;
  NsfCommandPreserve(cmd);
  NsfCommandRelease(del);
}

#if NSF_DEBUGGING
/** for debug purposes only */
static void CmdListPrint(Tcl_Interp *interp, const char *title, NsfCmdList *cmdList)
  nonnull(1) nonnull(3);

static void
CmdListPrint(Tcl_Interp *interp, const char *title, NsfCmdList *cmdList) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdList != NULL);

  if (title != NULL) {
    fprintf(stderr, "%s %p:\n", title, cmdList);
  }
  while (cmdList) {
    fprintf(stderr, "   CL=%p, cmdPtr=%p %s, clorobj %p, clientData=%p\n",
            cmdList,
            cmdList->cmdPtr, (interp != NULL) ? Tcl_GetCommandName(interp, cmdList->cmdPtr) : "",
            cmdList->clorobj,
            cmdList->clientData);
    cmdList = cmdList->nextPtr;
  }
}
#endif

/*
 * physically delete an entry 'del'
 */
static void CmdListDeleteCmdListEntry(NsfCmdList *del, NsfFreeCmdListClientData *freeFct)
  nonnull(1);

static void
CmdListDeleteCmdListEntry(NsfCmdList *del, NsfFreeCmdListClientData *freeFct) {

  nonnull_assert(del != NULL);

  if (unlikely(freeFct != NULL)) {
    (*freeFct)(del);
  }
  NsfCommandRelease(del->cmdPtr);
  FREE(NsfCmdList, del);
}

/*
 * remove a command 'delCL' from a command list, but do not
 * free it ... returns the removed NsfCmdList*
 */
static NsfCmdList *CmdListRemoveFromList(NsfCmdList **cmdList, NsfCmdList *delCL)
  nonnull(1) nonnull(2);

static NsfCmdList *
CmdListRemoveFromList(NsfCmdList **cmdList, NsfCmdList *delCL) {
  register NsfCmdList *c;
  NsfCmdList *del = NULL;

  nonnull_assert(cmdList != NULL);
  nonnull_assert(delCL != NULL);

  c = *cmdList;
  if (c == NULL) {
    return NULL;
  }

  if (c == delCL) {
    *cmdList = c->nextPtr;
    del = c;
  } else {
    while ((c->nextPtr != NULL) && (c->nextPtr != delCL)) {
      c = c->nextPtr;
    }
    if (c->nextPtr == delCL) {
      del = delCL;
      c->nextPtr = delCL->nextPtr;
    }
  }
  return del;
}

/*
 *----------------------------------------------------------------------
 * CmdListRemoveDeleted --
 *
 *    Remove all command pointers from a command list which are marked
 *    "deleted". The condition for deletion is the presence of the flag
 *    CMD_IS_DELETED, with the flag bit being set by
 *    Tcl_DeleteCommandFromToken().
 *
 * Results:
 *    The cmd list filtered for non-deleted commands
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void CmdListRemoveDeleted(NsfCmdList **cmdList, NsfFreeCmdListClientData *freeFct) nonnull(1) nonnull(2);

static void
CmdListRemoveDeleted(NsfCmdList **cmdList, NsfFreeCmdListClientData *freeFct) {
  NsfCmdList *f, *del;

  nonnull_assert(cmdList != NULL);
  nonnull_assert(freeFct != NULL);

  f = *cmdList;
  while (f) {
    /*
     * HIDDEN OBJECTS: For supporting hidden mixins, we cannot rely on the
     * cmdEpoch as indicator of the deletion status of a cmd because the epoch
     * counters of hidden and re-exposed commands are bumped. Despite of this,
     * their object structures remain valid. We resort to the use of the
     * per-cmd flag CMD_IS_DELETED, set upon processing a command in
     * Tcl_DeleteCommandFromToken().
     */
    if (Tcl_Command_flags(f->cmdPtr) & CMD_IS_DELETED /* Tcl_Command_cmdEpoch(f->cmdPtr) */) {
      del = f;
      f = f->nextPtr;
      del = CmdListRemoveFromList(cmdList, del);
      CmdListDeleteCmdListEntry(del, freeFct);
    } else
      f = f->nextPtr;
  }
}


/*
 * delete all cmds with given context class object
 */
static void CmdListRemoveContextClassFromList(NsfCmdList **cmdList, NsfClass *clorobj,
                                  NsfFreeCmdListClientData *freeFct) nonnull(1) nonnull(2) nonnull(3);

static void
CmdListRemoveContextClassFromList(NsfCmdList **cmdList, NsfClass *clorobj,
                                  NsfFreeCmdListClientData *freeFct) {
  NsfCmdList *c, *del = NULL;

  nonnull_assert(cmdList != NULL);
  nonnull_assert(clorobj != NULL);
  nonnull_assert(freeFct != NULL);

  /*
    CmdListRemoveDeleted(cmdList, freeFct);
  */
  c = *cmdList;
  while (c && c->clorobj == clorobj) {
    del = c;
    *cmdList = c->nextPtr;
    CmdListDeleteCmdListEntry(del, freeFct);
    c = *cmdList;
  }

  while (c) {
    if (c->clorobj == clorobj) {
      del = c;
      c = *cmdList;
      while ((c->nextPtr != NULL) && (c->nextPtr != del)) {
        c = c->nextPtr;
      }
      if (c->nextPtr == del) {
        c->nextPtr = del->nextPtr;
      }
      CmdListDeleteCmdListEntry(del, freeFct);
    }
    c = c->nextPtr;
  }
}

/*
 * free the memory of a whole 'cmdList'
 */
static void
CmdListFree(NsfCmdList **cmdList, NsfFreeCmdListClientData *freeFct) {

  nonnull_assert(cmdList != NULL);

  while (*cmdList) {
    NsfCmdList *del = *cmdList;
    *cmdList = (*cmdList)->nextPtr;
    CmdListDeleteCmdListEntry(del, freeFct);
  }
}

/*
 * simple list search proc to search a list of cmds
 * for a command ptr
 */
static NsfCmdList * CmdListFindCmdInList(Tcl_Command cmd, NsfCmdList *l) nonnull(2) nonnull(1);

static NsfCmdList *
CmdListFindCmdInList(Tcl_Command cmd, NsfCmdList *l) {
  register NsfCmdList *h;

  nonnull_assert(cmd != NULL);
  nonnull_assert(l != NULL);

  for (h = l; h; h = h->nextPtr) {
    if (h->cmdPtr == cmd) {
      return h;
    }
  }
  return NULL;
}

/*
 * simple list search proc to search a list of cmds
 * for a simple Name
 */
static NsfCmdList * CmdListFindNameInList(Tcl_Interp *interp, const char *name, NsfCmdList *l)
  nonnull(1) nonnull(2) nonnull(3);

static NsfCmdList *
CmdListFindNameInList(Tcl_Interp *interp, const char *name, NsfCmdList *cmdList) {

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  nonnull_assert(cmdList != NULL);

  do {
    const char *cmdName = Tcl_GetCommandName(interp, cmdList->cmdPtr);
    if (cmdName[0] == name[0] && strcmp(cmdName, name) == 0) {
      return cmdList;
    }
    cmdList = cmdList->nextPtr;
  } while (cmdList != NULL);

  return NULL;
}

/*
 *----------------------------------------------------------------------
 * CheckConditionInScope --
 *
 *    Check a given condition in the current call-frame's scope. It is
 *    the responsibility of the caller to push the intended call-frame.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int CheckConditionInScope(Tcl_Interp *interp, Tcl_Obj *condition) nonnull(1) nonnull(2);

static int
CheckConditionInScope(Tcl_Interp *interp, Tcl_Obj *condition) {
  int result, success;
  Tcl_Obj *ov[2] = {NULL, condition};

  nonnull_assert(interp != NULL);
  nonnull_assert(condition != NULL);

  INCR_REF_COUNT(condition);
  result = Nsf_ExprObjCmd(NULL, interp, 2, ov);
  DECR_REF_COUNT(condition);

  if (likely(result == TCL_OK)) {
    result = Tcl_GetBooleanFromObj(interp, Tcl_GetObjResult(interp), &success);
    if (result == TCL_OK && success == 0) {
      result = NSF_CHECK_FAILED;
    }
  }
  return result;
}

/*
 * Generic Obj-List handling functions.
 */

/*
 *----------------------------------------------------------------------
 * TclObjListFreeList --
 *
 *    Free the elements of the obj list.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    free memory.
 *
 *----------------------------------------------------------------------
 */
static void TclObjListFreeList(NsfTclObjList *list) nonnull(1);

static void
TclObjListFreeList(NsfTclObjList *list) {

  nonnull_assert(list != NULL);

  do {
    NsfTclObjList *del = list;

    list = list->nextPtr;
    DECR_REF_COUNT2("listContent", del->content);
    if (del->payload != NULL) {
      DECR_REF_COUNT2("listPayload", del->payload);
    }
    FREE(NsfTclObjList, del);
  } while (list != NULL);
}

/*
 *----------------------------------------------------------------------
 * TclObjListNewElement --
 *
 *    Add a new element to the obj list with an optional value (stored in
 *    payload).
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    allocate memory.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj * TclObjListNewElement(NsfTclObjList **list, Tcl_Obj *obj, Tcl_Obj *value)
  nonnull(1) nonnull(2) returns_nonnull;

static Tcl_Obj *
TclObjListNewElement(NsfTclObjList **list, Tcl_Obj *obj, Tcl_Obj *valueObj) {
  NsfTclObjList *elt = NEW(NsfTclObjList);

  nonnull_assert(list != NULL);
  nonnull_assert(obj != NULL);

  INCR_REF_COUNT2("listContent", obj);
  elt->content = obj;
  elt->payload = valueObj;
  if (valueObj != NULL) {
    INCR_REF_COUNT2("listPayload", valueObj);
  }
  elt->nextPtr = *list;
  *list = elt;

  return obj;
}

/*
 *----------------------------------------------------------------------
 * TclObjListAdd --
 *
 *    Add a NsfTclObjList element to the obj list indexed by a key into a
 *    sorted list of elements. Duplicates are appended to the payload
 *    elements.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Add element to the obj-list.
 *
 *----------------------------------------------------------------------
 */
static void TclObjListAdd(Tcl_Interp *interp, NsfTclObjList **list, Tcl_Obj *key, Tcl_Obj *value)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
TclObjListAdd(Tcl_Interp *interp, NsfTclObjList **list, Tcl_Obj *key, Tcl_Obj *value) {
  NsfTclObjList *elt, **prevPtr;
  const char *keyString = ObjStr(key);

  nonnull_assert(interp != NULL);
  nonnull_assert(list != NULL);
  nonnull_assert(key != NULL);
  nonnull_assert(value != NULL);

  for (elt = *list, prevPtr = list; elt; prevPtr = &elt->nextPtr, elt = elt->nextPtr) {
    const char *eltString = ObjStr(elt->content);

    if (key == elt->content || strcmp(keyString, eltString) == 0) {
      /*
       * Found the element, append to it
       */
      /* fprintf(stderr, "TclObjListAdd: insert %s/%s equal, append to %s\n",
         keyString, ObjStr(value), ObjStr(elt->payload));*/
      Tcl_ListObjAppendElement(interp, elt->payload, value);
      return;
    }
    if (strcmp(keyString, eltString) < 0) {
      /*
       * Element not found, insert new before as a new entry.
       */
      /*fprintf(stderr, "TclObjListAdd: insert %s/%s before %s isshared %d\n",
        keyString, ObjStr(value), eltString, Tcl_IsShared(key));*/
      TclObjListNewElement(prevPtr, key, Tcl_IsShared(value) ? Tcl_DuplicateObj(value) : value);
      return;
    }
  }
  /*
   * Element not found, insert new as last entry.
   */
  /* fprintf(stderr, "TclObjListAdd: insert last %s value %s\n", keyString, ObjStr(value)); */
  TclObjListNewElement(prevPtr, key, Tcl_NewListObj(1, &value));

  return;
}

#if defined(NSF_WITH_ASSERTIONS)
/*********************************************************************
 * Assertions
 **********************************************************************/

static NsfTclObjList * AssertionNewList(Tcl_Interp *interp, Tcl_Obj *aObj) nonnull(1);

static NsfTclObjList *
AssertionNewList(Tcl_Interp *interp, Tcl_Obj *aObj) {
  Tcl_Obj **ov; int oc;
  NsfTclObjList *last = NULL;

  nonnull_assert(interp != NULL);

  if (aObj && Tcl_ListObjGetElements(interp, aObj, &oc, &ov) == TCL_OK) {
    if (oc > 0) {
      int i;
      for (i = oc - 1; i >= 0; i--) {
        TclObjListNewElement(&last, ov[i], NULL);
      }
    }
  }
  return last;
}

static Tcl_Obj *AssertionList(Tcl_Interp *interp, NsfTclObjList *alist) nonnull(1);

static Tcl_Obj *
AssertionList(Tcl_Interp *interp, NsfTclObjList *alist) {
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);

  for (; alist; alist = alist->nextPtr) {
    Tcl_ListObjAppendElement(interp, listObj, alist->content);
  }
  return listObj;
}

static int AssertionListCheckOption(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static int
AssertionListCheckOption(Tcl_Interp *interp, NsfObject *object) {
  NsfObjectOpt *opt = object->opt;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (opt == 0) {
    return TCL_OK;
  }

  resultObj = Tcl_GetObjResult(interp);

  if (opt->checkoptions & CHECK_OBJINVAR) {
    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("object-invar", -1));
  }
  if (opt->checkoptions & CHECK_CLINVAR) {
    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("class-invar", -1));
  }
  if (opt->checkoptions & CHECK_PRE) {
    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("pre", -1));
  }
  if (opt->checkoptions & CHECK_POST) {
    Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("post", -1));
  }

  return TCL_OK;
}

static NsfProcAssertion *AssertionFindProcs(NsfAssertionStore *aStore, const char *name)
  nonnull(1) nonnull(2);

static NsfProcAssertion *
AssertionFindProcs(NsfAssertionStore *aStore, const char *name) {
  Tcl_HashEntry *hPtr;

  nonnull_assert(aStore != NULL);
  nonnull_assert(name != NULL);

  hPtr = Tcl_CreateHashEntry(&aStore->procs, name, NULL);
  if (hPtr == NULL) {
    return NULL;
  }
  return (NsfProcAssertion *) Tcl_GetHashValue(hPtr);
}

static void AssertionRemoveProc(NsfAssertionStore *aStore, const char *name) nonnull(1) nonnull(2);

static void
AssertionRemoveProc(NsfAssertionStore *aStore, const char *name) {
  Tcl_HashEntry *hPtr;

  nonnull_assert(aStore != NULL);
  nonnull_assert(name != NULL);

  hPtr = Tcl_CreateHashEntry(&aStore->procs, name, NULL);
  if (hPtr != NULL) {
    NsfProcAssertion *procAss = (NsfProcAssertion *) Tcl_GetHashValue(hPtr);

    if (procAss->pre != NULL) {
      TclObjListFreeList(procAss->pre);
    }
    if (procAss->post != NULL) {
      TclObjListFreeList(procAss->post);
    }
    FREE(NsfProcAssertion, procAss);
    Tcl_DeleteHashEntry(hPtr);
  }
}

static void AssertionAddProc(Tcl_Interp *interp, const char *name, NsfAssertionStore *aStore,
                 Tcl_Obj *pre, Tcl_Obj *post) nonnull(1) nonnull(2) nonnull(3);

static void
AssertionAddProc(Tcl_Interp *interp, const char *name, NsfAssertionStore *aStore,
                 Tcl_Obj *pre, Tcl_Obj *post) {
  int new = 0;
  Tcl_HashEntry *hPtr = NULL;
  NsfProcAssertion *procs = NEW(NsfProcAssertion);

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);
  nonnull_assert(aStore != NULL);

  AssertionRemoveProc(aStore, name);
  procs->pre = AssertionNewList(interp, pre);
  procs->post = AssertionNewList(interp, post);
  hPtr = Tcl_CreateHashEntry(&aStore->procs, name, &new);
  if (new != 0) {
    Tcl_SetHashValue(hPtr, procs);
  }
}

static NsfAssertionStore *AssertionCreateStore() returns_nonnull;

static NsfAssertionStore *
AssertionCreateStore() {
  NsfAssertionStore *aStore = NEW(NsfAssertionStore);

  aStore->invariants = NULL;
  Tcl_InitHashTable(&aStore->procs, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable", &aStore->procs);
  return aStore;
}

static void AssertionRemoveStore(NsfAssertionStore *aStore) nonnull(1);

static void
AssertionRemoveStore(NsfAssertionStore *aStore) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;

  nonnull_assert(aStore != NULL);

  for (hPtr = Tcl_FirstHashEntry(&aStore->procs, &hSrch); hPtr;
       hPtr = Tcl_FirstHashEntry(&aStore->procs, &hSrch)) {
    /*
     * AssertionRemoveProc calls Tcl_DeleteHashEntry(hPtr), thus
     * we get the FirstHashEntry afterwards again to proceed
     */
    AssertionRemoveProc(aStore, Tcl_GetHashKey(&aStore->procs, hPtr));
  }
  Tcl_DeleteHashTable(&aStore->procs);
  MEM_COUNT_FREE("Tcl_InitHashTable", &aStore->procs);
  if (aStore->invariants != NULL) {
    TclObjListFreeList(aStore->invariants);
  }
  FREE(NsfAssertionStore, aStore);
}

static int AssertionCheckList(Tcl_Interp *interp, NsfObject *object,
                              NsfTclObjList *alist, const char *methodName)
  nonnull(1) nonnull(2) nonnull(4);

static int
AssertionCheckList(Tcl_Interp *interp, NsfObject *object,
                   NsfTclObjList *alist, const char *methodName) {
  NsfTclObjList *checkFailed = NULL;
  Tcl_Obj *savedResultObj = Tcl_GetObjResult(interp);
  int savedCheckoptions, acResult = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);

  /*
   * no obj->opt -> checkoption == CHECK_NONE
   */
  if (object->opt == NULL) {
    return TCL_OK;
  }
  /* we do not check assertion modifying methods, otherwise
     we cannot react in catch on a runtime assertion check failure */

#if 1
  /* TODO: the following check operations is XOTcl1 legacy and is not
     generic. it should be replaced by another method-property.
     Most of the is*String()
     definition are then obsolete and should be deleted from
     nsfInt.h as well.
  */

  if (isCheckString(methodName)) {
    return TCL_OK;
  }
#endif

  INCR_REF_COUNT(savedResultObj);

  Tcl_ResetResult(interp);

  while (alist) {
    /* Eval instead of IfObjCmd => the substitutions in the
       conditions will be done by Tcl */
    const char *assStr = ObjStr(alist->content), *c = assStr;
    int comment = 0;

    for (; c && *c != '\0'; c++) {
      if (*c == '#') {
        comment = 1; break;
      }
    }

    if (comment == 0) {
      CallFrame frame, *framePtr = &frame;
      Nsf_PushFrameObj(interp, object, framePtr);

      /* don't check assertion during assertion check */
      savedCheckoptions = object->opt->checkoptions;
      object->opt->checkoptions = CHECK_NONE;

      /* fprintf(stderr, "Checking Assertion %s ", assStr); */

      /*
       * now check the assertion in the pushed call-frame's scope
       */
      acResult = CheckConditionInScope(interp, alist->content);
      if (acResult != TCL_OK) {
        checkFailed = alist;
      }
      object->opt->checkoptions = savedCheckoptions;
      /* fprintf(stderr, "...%s\n", (checkFailed != 0) ? "failed" : "ok"); */
      Nsf_PopFrameObj(interp, framePtr);
    }
    if (checkFailed != 0) {
      break;
    }
    alist = alist->nextPtr;
  }

  if (unlikely(checkFailed != 0)) {
    DECR_REF_COUNT(savedResultObj);
    if (acResult == TCL_ERROR) {
      Tcl_Obj *sr = Tcl_GetObjResult(interp);
      INCR_REF_COUNT(sr);
      NsfPrintError(interp, "error in Assertion: {%s} in proc '%s'\n%s",
                    ObjStr(checkFailed->content), methodName, ObjStr(sr));
      DECR_REF_COUNT(sr);
      return TCL_ERROR;
    }
    return NsfPrintError(interp, "assertion failed check: {%s} in proc '%s'",
                         ObjStr(checkFailed->content), methodName);
  }

  Tcl_SetObjResult(interp, savedResultObj);
  DECR_REF_COUNT(savedResultObj);
  return TCL_OK;
}

static int AssertionCheckInvars(Tcl_Interp *interp, NsfObject *object,
                     const char *methodName,
                     CheckOptions checkoptions)
  nonnull(1) nonnull(2) nonnull(3);

static int
AssertionCheckInvars(Tcl_Interp *interp, NsfObject *object,
                     const char *methodName,
                     CheckOptions checkoptions) {
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);

  if (checkoptions & CHECK_OBJINVAR && object->opt->assertions) {
    result = AssertionCheckList(interp, object, object->opt->assertions->invariants,
                                methodName);
  }

  if (result != TCL_ERROR && checkoptions & CHECK_CLINVAR) {
    NsfClasses *clPtr;

    clPtr = PrecedenceOrder(object->cl);
    while ((clPtr != NULL) && (result != TCL_ERROR)) {
      NsfAssertionStore *aStore = (clPtr->cl->opt != NULL) ? clPtr->cl->opt->assertions : NULL;

      if (aStore != NULL) {
        result = AssertionCheckList(interp, object, aStore->invariants, methodName);
      }
      clPtr = clPtr->nextPtr;
    }
  }
  return result;
}

static int AssertionCheck(Tcl_Interp *interp, NsfObject *object, NsfClass *cl,
               const char *method, int checkOption)
  nonnull(1) nonnull(2) nonnull(4);

static int
AssertionCheck(Tcl_Interp *interp, NsfObject *object, NsfClass *cl,
               const char *method, int checkOption) {
  int result = TCL_OK;
  NsfAssertionStore *aStore;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(method != NULL);
  assert(object->opt != NULL);

  if (cl != NULL) {
    aStore = (cl->opt != NULL) ? cl->opt->assertions : NULL;
  } else {
    aStore = (object->opt != NULL) ? object->opt->assertions : NULL;
  }

  if (aStore && (checkOption & object->opt->checkoptions)) {
    NsfProcAssertion *procs = AssertionFindProcs(aStore, method);

    if (procs != NULL) {
      switch (checkOption) {
      case CHECK_PRE:
        result = AssertionCheckList(interp, object, procs->pre, method);
        break;
      case CHECK_POST:
        result = AssertionCheckList(interp, object, procs->post, method);
        break;
      }
    }
    if (likely(result != TCL_ERROR)) {
      result = AssertionCheckInvars(interp, object, method, object->opt->checkoptions);
    }
  }
  return result;
}

static int AssertionSetCheckOptions(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *arg)
  nonnull(1) nonnull(2) nonnull(3);

static int
AssertionSetCheckOptions(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *arg) {
  NsfObjectOpt *opt = NsfRequireObjectOpt(object);
  int ocArgs;
  Tcl_Obj **ovArgs;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(arg != NULL);

  opt->checkoptions = CHECK_NONE;

  if (Tcl_ListObjGetElements(interp, arg, &ocArgs, &ovArgs) == TCL_OK
      && ocArgs > 0) {
    int i;
    for (i = 0; i < ocArgs; i++) {
      const char *option = ObjStr(ovArgs[i]);
      if (option != NULL) {
        switch (*option) {
        case 'c':
          if (strcmp(option, "class-invar") == 0) {
            opt->checkoptions |= CHECK_CLINVAR;
          }
          break;
        case 'o':
          if (strcmp(option, "object-invar") == 0) {
            opt->checkoptions |= CHECK_OBJINVAR;
          }
          break;
        case 'p':
          if (strcmp(option, "pre") == 0) {
            opt->checkoptions |= CHECK_PRE;
          } else if (strcmp(option, "post") == 0) {
            opt->checkoptions  |= CHECK_POST;
          }
          break;
        case 'a':
          if (strcmp(option, "all") == 0) {
            opt->checkoptions |= CHECK_ALL;
          }
          break;
        }
      }
    }
  }
  if (opt->checkoptions == CHECK_NONE && ocArgs > 0) {
    return NsfPrintError(interp, "unknown check option in command '%s' check %s, ",
                         "valid: all pre post object-invar class-invar",
                         ObjectName(object), ObjStr(arg));
  }
  return TCL_OK;
}

static void AssertionSetInvariants(Tcl_Interp *interp, NsfAssertionStore **assertions, Tcl_Obj *arg)
  nonnull(1) nonnull(2) nonnull(3);

static void
AssertionSetInvariants(Tcl_Interp *interp, NsfAssertionStore **assertions, Tcl_Obj *arg) {

  nonnull_assert(interp != NULL);
  nonnull_assert(assertions != NULL);
  nonnull_assert(arg != NULL);

  if (*assertions) {
    TclObjListFreeList((*assertions)->invariants);
  } else {
    *assertions = AssertionCreateStore();
  }
  (*assertions)->invariants = AssertionNewList(interp, arg);
}
#endif /* NSF_WITH_ASSERTIONS */




/***********************************************************************
 * Mixin support
 ***********************************************************************/

/*
 * push a mixin stack information on this object
 */
static int MixinStackPush(NsfObject *object) nonnull(1);

static int
MixinStackPush(NsfObject *object) {
  register NsfMixinStack *h = NEW(NsfMixinStack);

  nonnull_assert(object != NULL);

  h->currentCmdPtr = NULL;
  h->nextPtr = object->mixinStack;
  object->mixinStack = h;
  /*fprintf(stderr, "MixinStackPush %p %s\n", object, ObjectName(object));*/
  return 1;
}

/*
 * Pop a mixin stack information on this object.
 */
static void MixinStackPop(NsfObject *object) nonnull(1);

static void
MixinStackPop(NsfObject *object) {
  register NsfMixinStack *h;

  nonnull_assert(object != NULL);

  /*fprintf(stderr, "MixinStackPop %p %s\n", object, ObjectName(object));*/
  h = object->mixinStack;
  object->mixinStack = h->nextPtr;
  FREE(NsfMixinStack, h);
}

/*
 * Appends NsfClasses (containing the mixin-classes and their
 * super-classes) to 'mixinClasses' list from a given mixinList.
 */
static void MixinComputeOrderFullList(Tcl_Interp *interp, NsfCmdList **mixinList,
                          NsfClasses **mixinClasses,
                          NsfClasses **checkList, int level) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
MixinComputeOrderFullList(Tcl_Interp *interp, NsfCmdList **mixinList,
                          NsfClasses **mixinClasses,
                          NsfClasses **checkList, int level) {
  NsfCmdList *m;
  NsfClasses *pl, **clPtr = mixinClasses;

  nonnull_assert(interp != NULL);
  nonnull_assert(mixinList != NULL);
  nonnull_assert(mixinClasses != NULL);
  nonnull_assert(checkList != NULL);

  CmdListRemoveDeleted(mixinList, GuardDel);

  for (m = *mixinList; m; m = m->nextPtr) {
    NsfClass *mCl = NsfGetClassFromCmdPtr(m->cmdPtr);

    if (mCl != NULL) {
      for (pl = PrecedenceOrder(mCl); pl; pl = pl->nextPtr) {
        if (IsRootClass(pl->cl) == 0) {
          NsfClassOpt *opt = pl->cl->opt;

          /* fprintf(stderr, "find %p %s in checklist 1 %p\n",
             pl->cl, ClassName(pl->cl), *checkList);*/
          if (*checkList != NULL && NsfClassListFind(*checkList, pl->cl)) {
            /*fprintf(stderr, "+++ never add %s\n", ClassName(pl->cl));*/
          } else {
            if (opt != NULL && opt->classMixins != NULL) {
              /*
               * Compute transitively the (class) mixin-classes of this
               * added class.
               */
              NsfClassListAdd(checkList, pl->cl, NULL);
              /*fprintf(stderr, "+++ transitive %s\n", ClassName(pl->cl));*/
              MixinComputeOrderFullList(interp, &opt->classMixins, mixinClasses,
                                        checkList, level+1);
            }
            /*fprintf(stderr, "+++ add to mixinClasses %p path: %s clPtr %p\n",
              mixinClasses, ClassName(pl->cl), clPtr);*/
            clPtr = NsfClassListAddNoDup(clPtr, pl->cl, m->clientData, NULL);
          }
        }
      }
    }
  }

  if (level == 0 && *checkList) {
    NsfClassListFree(*checkList);
    *checkList = NULL;
  }
}

/*
 *----------------------------------------------------------------------
 * MixinResetOrder --
 *
 *    Free the mixin order of the provided object if it exists.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Frees potentially the mixinOrder list.
 *
 *----------------------------------------------------------------------
 */
static void MixinResetOrder(NsfObject *object) nonnull(1);

static void
MixinResetOrder(NsfObject *object) {

  nonnull_assert(object != NULL);

  CmdListFree(&object->mixinOrder, NULL /*GuardDel*/);
  object->mixinOrder = NULL;
}

/*
 *----------------------------------------------------------------------
 * NsfClassListAddPerClassMixins --
 *
 *    Append the class mixins to the provided list. CheckList is used to
 *    eliminate potential duplicates.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Appends potentially elements to classListPtr and checkList
 *
 *----------------------------------------------------------------------
 */
static void NsfClassListAddPerClassMixins(Tcl_Interp *interp, NsfClass *cl,
                              NsfClasses **classListPtr, NsfClasses **checkList)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
NsfClassListAddPerClassMixins(Tcl_Interp *interp, NsfClass *cl,
                              NsfClasses **classListPtr, NsfClasses **checkList) {
  NsfClasses *pl;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(classListPtr != NULL);
  nonnull_assert(checkList != NULL);

  for (pl = PrecedenceOrder(cl); pl; pl = pl->nextPtr) {
    NsfClassOpt *clopt = pl->cl->opt;
    if (clopt != NULL && clopt->classMixins) {
      MixinComputeOrderFullList(interp, &clopt->classMixins,
                                classListPtr, checkList, 1);
    }
  }
}

/*
 *----------------------------------------------------------------------
 * MixinComputeOrder --
 *
 *    Compute a duplicate-free linearized order of per-object and per-class
 *    mixins and the class inheritance. The precedence rule is that the last
 *    occurrence makes it into the final list.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    object->mixinOrder is updated.
 *
 *----------------------------------------------------------------------
 */
static void MixinComputeOrder(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static void
MixinComputeOrder(Tcl_Interp *interp, NsfObject *object) {
  NsfClasses *fullList, *checkList = NULL, *mixinClasses = NULL, *clPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->mixinOrder != NULL) {
    MixinResetOrder(object);
  }

  /* Append per-obj mixins */
  if (object->opt != NULL) {
    NsfCmdList *m;

    MixinComputeOrderFullList(interp, &object->opt->objMixins, &mixinClasses,
                              &checkList, 1);
    /*
     * Add per-object mixins to checkList to avoid that theses classes in the
     * class mixins.
     *
     * TODO: we could add this already in MixinComputeOrderFullList() if we
     * provide an additional flag.
     */
    for (m = object->opt->objMixins; m; m = m->nextPtr) {
      NsfClass *mCl = NsfGetClassFromCmdPtr(m->cmdPtr);
      if (mCl != NULL) {
        NsfClassListAddNoDup(&checkList, mCl, NULL, NULL);
      }
    }
  }
  /*fprintf(stderr, "%s ", ObjectName(object));
  NsfClassListPrint("MixinComputeOrder poms", mixinClasses);
  NsfClassListPrint("MixinComputeOrder poms checkList", checkList);*/

  /* append per-class mixins */
  NsfClassListAddPerClassMixins(interp, object->cl, &mixinClasses, &checkList);

  /*fprintf(stderr, "%s ", ObjectName(object));
  NsfClassListPrint("MixinComputeOrder poms+pcms", mixinClasses);
  CmdListPrint(interp, "mixinOrder", object->mixinOrder);*/

  if (checkList != NULL) {
    NsfClassListFree(checkList);
  }

  fullList = mixinClasses;

  /*
   * Don't add duplicates or classes of the precedence order to the resulting
   * list.
   */
  for (clPtr = mixinClasses; clPtr; clPtr = clPtr->nextPtr) {
    NsfClass *cl = clPtr->cl;
    NsfClasses *checker;

    /*fprintf(stderr, "--- Work on %s\n", ClassName(cl));
      CmdListPrint(interp, "mixinOrder", object->mixinOrder);*/

    checker = NsfClassListFind(clPtr->nextPtr, cl);

    /*
     * If checker is set, it is a duplicate and ignored.
     */
    if (checker == NULL) {
      /* check object->cl hierarchy */
      checker = NsfClassListFind(PrecedenceOrder(object->cl), cl);
      /*
       * If checker is set, it was found in the class hierarchy and it is
       * ignored.
       */
    }
    if (checker == NULL) {
      /* add the class to the mixinOrder list */
      NsfCmdList *new;

      /*fprintf(stderr, "--- adding to mixinOrder %s to cmdlist %p of object %s\n",
        ClassName(cl), object->mixinOrder, ObjectName(object));*/
      new = CmdListAdd(&object->mixinOrder, cl->object.id, NULL, /*noDuplicates*/ 0, 1);
      /*CmdListPrint(interp, "mixinOrder", object->mixinOrder);*/

      /*
       * We require the first matching guard of the full list in the new
       * client data
       */
      checker = NsfClassListFind(fullList, cl);
      if (checker != NULL) {
        new->clientData = checker->clientData;
      }
    }

  }

  /* ... and free the memory of the full list */
  if (fullList != NULL) {
    NsfClassListFree(fullList);
  }

  /*CmdListPrint(interp, "mixin order\n", obj->mixinOrder);*/
}


/*
 *----------------------------------------------------------------------
 * MixinAdd --
 *
 *    Add a mixinreg (mixin-class with a potential guard) provided as a
 *    Tcl_Obj* to 'mixinList' by appending it to the provided cmdList.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Potentially allocating cmd list elements added to the mixinList
 *
 *----------------------------------------------------------------------
 */
static int MixinAdd(Tcl_Interp *interp, NsfCmdList **mixinList, Tcl_Obj *nameObj, NsfClass *baseClass)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
MixinAdd(Tcl_Interp *interp, NsfCmdList **mixinList, Tcl_Obj *nameObj, NsfClass *baseClass) {
  NsfClass *mixinCl;
  Tcl_Obj *guardObj;
  NsfCmdList *new;

  nonnull_assert(interp != NULL);
  nonnull_assert(mixinList != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(baseClass != NULL);

  /*fprintf(stderr, "MixinAdd gets obj %p type %p %s\n", nameObj, nameObj->typePtr, (nameObj->typePtr != NULL) ?nameObj->typePtr->name : "NULL");*/

  /*
   * When the provided nameObj is of type NsfMixinregObjType, the nsf specific
   * converter was called already; otherwise call the converter here.
   */
  if (nameObj->typePtr != &NsfMixinregObjType) {
    if (Tcl_ConvertToType(interp, nameObj, &NsfMixinregObjType) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  NsfMixinregGet(interp, nameObj, &mixinCl, &guardObj);

  assert((Tcl_Command_flags(mixinCl->object.id) & CMD_IS_DELETED) == 0);

  new = CmdListAdd(mixinList, mixinCl->object.id, NULL, /*noDuplicates*/ 1, 1);

  if (guardObj != NULL) {
    GuardAdd(new, guardObj);
  } else if (new->clientData != NULL) {
    GuardDel(new);
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * AppendMatchingElement --
 *
 *    Call AppendElement to the resultObj for values matching the specified
 *    pattern.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Appends element to the result object
 *
 *----------------------------------------------------------------------
 */
static void AppendMatchingElement(Tcl_Interp *interp, Tcl_Obj *resultObj, Tcl_Obj *nameObj, const char *pattern)
  nonnull(1) nonnull(2) nonnull(3);

static void
AppendMatchingElement(Tcl_Interp *interp, Tcl_Obj *resultObj, Tcl_Obj *nameObj, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(resultObj != NULL);
  nonnull_assert(nameObj != NULL);

  if (pattern == NULL || Tcl_StringMatch( ObjStr(nameObj), pattern)) {
    Tcl_ListObjAppendElement(interp, resultObj, nameObj);
  }
}

/*
 *----------------------------------------------------------------------
 * AppendMatchingElementsFromCmdList --
 *
 *    Apply AppendMatchingElement() to all elements of the passed
 *    Cmdlist
 *
 * Results:
 *    1 iff a matching object was provided and it was found; 0 otherwise
 *
 * Side effects:
 *    Appends elements to the result
 *
 *----------------------------------------------------------------------
 */
static int AppendMatchingElementsFromCmdList(Tcl_Interp *interp, NsfCmdList *cmdl,
                                  Tcl_Obj *resultObj,
                                  const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3);

static int
AppendMatchingElementsFromCmdList(Tcl_Interp *interp, NsfCmdList *cmdList,
                                  Tcl_Obj *resultObj,
                                  const char *pattern, NsfObject *matchObject) {
  int rc = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdList != NULL);
  nonnull_assert(resultObj != NULL);

  do {
    NsfObject *object = NsfGetObjectFromCmdPtr(cmdList->cmdPtr);
    if (object != NULL) {
      if (matchObject == object) {
        return 1;
      } else {
        AppendMatchingElement(interp, resultObj, object->cmdName, pattern);
      }
    }
    cmdList = cmdList->nextPtr;
  } while (cmdList != NULL);

  return rc;
}

/*
 *----------------------------------------------------------------------
 * AppendMatchingElementsFromClasses --
 *
 *    Apply AppendMatchingElement() to all elements of the passed
 *    class list
 *
 * Results:
 *    1 iff a matching object was provided and it was found; 0 otherwise
 *
 * Side effects:
 *    Appends elements to the result
 *
 *----------------------------------------------------------------------
 */
static int AppendMatchingElementsFromClasses(Tcl_Interp *interp, NsfClasses *cls,
                                             const char *pattern, NsfObject *matchObject)
  nonnull(1);

static int
AppendMatchingElementsFromClasses(Tcl_Interp *interp, NsfClasses *cls,
                                  const char *pattern, NsfObject *matchObject) {
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);

  resultObj = Tcl_GetObjResult(interp);
  for ( ; cls != NULL; cls = cls->nextPtr) {
    NsfObject *object = (NsfObject *)cls->cl;

    if (object != NULL) {
      if (matchObject != NULL && object == matchObject) {
        /*
         * We have a matchObject and it is identical to obj,
         * just return true and don't continue search
         */
        return 1;
      } else {
        AppendMatchingElement(interp, resultObj, object->cmdName, pattern);
      }
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetAllInstances --
 *
 *    Get all instances of a class recursively into an initialized
 *    String key hash-table
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Passed hash-table contains instances
 *
 *----------------------------------------------------------------------
 */
static void
GetAllInstances(Tcl_Interp *interp, NsfCmdList **instances, NsfClass *startCl) {
  NsfClasses *clPtr, *subClasses = TransitiveSubClasses(startCl);

  nonnull_assert(interp != NULL);
  nonnull_assert(instances != NULL);
  nonnull_assert(startCl != NULL);

  for (clPtr = subClasses; clPtr; clPtr = clPtr->nextPtr) {
    Tcl_HashTable *tablePtr = &clPtr->cl->instances;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search);  hPtr;
         hPtr = Tcl_NextHashEntry(&search)) {
      NsfObject *inst = (NsfObject *)Tcl_GetHashKey(tablePtr, hPtr);
      Command *cmdPtr;

      if (unlikely((inst->flags & NSF_TCL_DELETE) != 0u)) {
        NsfLog(interp, NSF_LOG_NOTICE, "Object %s is apparently deleted", ObjectName(inst));
        continue;
      }

      cmdPtr = (Command *)inst->id;
      assert(cmdPtr != NULL);

      if (unlikely((cmdPtr->nsPtr->flags & NS_DYING) != 0u)) {
        NsfLog(interp, NSF_LOG_WARN, "Namespace of %s is apparently deleted", ObjectName(inst));
        continue;
      }

#if !defined(NDEBUG)
      {
        /*
         * Make sure, we can still lookup the object; the object has to be still
         * alive.
         */
        NsfObject *object = GetObjectFromString(interp, ObjectName(inst));
        /*
         * HIDDEN OBJECTS: Provide a fallback to a pointer-based lookup. This is
         * needed because objects can be hidden or re-exposed under a different
         * name which is not reported back to the object system by the [interp
         * hide|expose] mechanism. However, we still want to process hidden and
         * re-exposed objects during cleanup like ordinary, exposed ones.
         */
        if (unlikely(object == NULL)) {
          object = GetHiddenObjectFromCmd(interp, inst->id);
        }
        assert(object != NULL);
      }
#endif

      /*fprintf (stderr, " -- %p flags %.6x activation %d %s id %p id->flags %.6x "
        "nsPtr->flags %.6x (instance of %s)\n",
        inst, inst->flags, inst->activationCount,
        ObjectName(inst), inst->id, cmdPtr->flags, (cmdPtr->nsPtr != NULL) ? cmdPtr->nsPtr->flags : 0,
        ClassName(clPtr->cl));*/

      CmdListAdd(instances, inst->id, (NsfClass *)inst, 0, 0);
    }
  }

  if (subClasses != NULL) {
    NsfClassListFree(subClasses);
  }
}

/*
 *----------------------------------------------------------------------
 * AddToResultSet --
 *
 *    Helper function to add classes to the result set (implemented as
 *    a hash-table), flagging test for matchObject as result
 *
 * Results:
 *    1 iff a matching object was provided and it was found; 0 otherwise
 *
 * Side effects:
 *    Appends optionally element to the result object
 *
 *----------------------------------------------------------------------
 */
static int AddToResultSet(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
               Tcl_Obj *resultSet, NsfObject *object, int *new,
               int appendResult, const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

static int
AddToResultSet(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
               Tcl_Obj *resultSet, NsfObject *object, int *new,
               int appendResult, const char *pattern, NsfObject *matchObject) {

  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(resultSet != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(new != NULL);

  Tcl_CreateHashEntry(destTablePtr, (char *)object, new);
  if (*new) {
    if (matchObject != NULL && matchObject == object) {
      return 1;
    }
    if (appendResult != 0) {
      AppendMatchingElement(interp, resultSet, object->cmdName, pattern);
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * AddToResultSetWithGuards --
 *
 *    Helper function to add classes with guards to the result set
 *    (implemented as a hash-table, full version as a Tcl list), flagging test
 *    for matchObject as result.
 *
 * Results:
 *    1 iff a matching object was provided and it was found; 0 otherwise
 *
 * Side effects:
 *    Appends optionally element to the result object
 *
 *----------------------------------------------------------------------
 */
static int AddToResultSetWithGuards(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                         Tcl_Obj *resultSet, NsfClass *cl,
                         ClientData clientData, int *new, int appendResult,
                         const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(6) nonnull(5);

static int
AddToResultSetWithGuards(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                         Tcl_Obj *resultSet, NsfClass *cl,
                         ClientData clientData, int *new, int appendResult,
                         const char *pattern, NsfObject *matchObject) {

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(resultSet != NULL);
  nonnull_assert(new != NULL);

  Tcl_CreateHashEntry(destTablePtr, (char *)cl, new);
  if (*new) {
    if (appendResult != 0) {
      if (pattern == NULL || Tcl_StringMatch(ClassName(cl), pattern)) {
        Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
        Tcl_Obj *g = (Tcl_Obj *) clientData;
        INCR_REF_COUNT(listObj);
        Tcl_ListObjAppendElement(interp, listObj, cl->object.cmdName);
        Tcl_ListObjAppendElement(interp, listObj, NsfGlobalObjs[NSF_GUARD_OPTION]);
        Tcl_ListObjAppendElement(interp, listObj, g);
        Tcl_ListObjAppendElement(interp, resultSet, listObj);
        DECR_REF_COUNT(listObj);
      }
    }
    if (matchObject != NULL && matchObject == (NsfObject *)cl) {
      return 1;
    }
  }

  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetAllObjectMixinsOf --
 *
 *    Computes a set of classes, into which this class was mixed in
 *    via per object mixin. The function gets recursively all per
 *    object mixins from an class and its subclasses/isClassMixinOf
 *    and adds it into an initialized object ptr hash-table
 *    (TCL_ONE_WORD_KEYS)
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The set of classes is returned in the provided hash-table
 *
 *----------------------------------------------------------------------
 */
static int GetAllObjectMixinsOf(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                     Tcl_Obj *resultSet, NsfClass *startCl, int isMixin,
                     int appendResult, const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
GetAllObjectMixinsOf(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                     Tcl_Obj *resultSet, NsfClass *startCl, int isMixin,
                     int appendResult, const char *pattern, NsfObject *matchObject) {
  int rc = 0, new = 0;
  NsfClasses *sc;

  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(resultSet != NULL);
  nonnull_assert(startCl != NULL);

  /*fprintf(stderr, "startCl = %s, opt %p, isMixin %d, pattern '%s', matchObject %p\n",
    ClassName(startCl), startCl->opt, isMixin, pattern, matchObject);*/

  /*
   * check all subclasses of startCl for mixins
   */
  for (sc = startCl->sub; sc; sc = sc->nextPtr) {
    rc = GetAllObjectMixinsOf(interp, destTablePtr, resultSet,
                              sc->cl, isMixin, appendResult,
                              pattern, matchObject);
    if (rc != 0) {return rc;}
  }
  /*fprintf(stderr, "check subclasses of %s done\n", ClassName(startCl));*/

  if (startCl->opt != NULL) {
    NsfCmdList *m;

    for (m = startCl->opt->isClassMixinOf; m; m = m->nextPtr) {
      NsfClass *cl;

      /* we should have no deleted commands in the list */
      assert((Tcl_Command_flags(m->cmdPtr) & CMD_IS_DELETED) == 0);

      cl = NsfGetClassFromCmdPtr(m->cmdPtr);
      assert(cl != NULL);
      /*fprintf(stderr, "check %s mixinof %s\n", ClassName(cl), ClassName((startCl)));*/
      rc = GetAllObjectMixinsOf(interp, destTablePtr, resultSet,
                                cl, isMixin, appendResult,
                                pattern, matchObject);
      /* fprintf(stderr, "check %s mixinof %s done\n",
         ClassName(cl), ClassName(startCl));*/
      if (rc != 0) {return rc;}
    }
  }

  /*
   * check, if startCl has associated per-object mixins
   */
  if (startCl->opt != NULL) {
    NsfCmdList *m;

    for (m = startCl->opt->isObjectMixinOf; m; m = m->nextPtr) {
      NsfObject *object;

      /* we should have no deleted commands in the list */
      assert((Tcl_Command_flags(m->cmdPtr) & CMD_IS_DELETED) == 0);

      object = NsfGetObjectFromCmdPtr(m->cmdPtr);
      assert(object != NULL);

      rc = AddToResultSet(interp, destTablePtr, resultSet,
                          object, &new, appendResult,
                          pattern, matchObject);
      if (rc == 1) {return rc;}
    }
  }
  return rc;
}

/*
 *----------------------------------------------------------------------
 * AddClassListEntriesToMixinsOfSet --
 *
 *    Helper function of GetAllClassMixinsOf(). Iterate over the provided
 *    class list (mixinOfs) and add every entry to the result set. If the
 *    entry is new, GetAllClassMixinsOf() is called recursively.
 *
 * Results:
 *    1 in case the operation is finished.
 *
 * Side effects:
 *    The set of classes is returned in the provided hash-table
 *
 *----------------------------------------------------------------------
 */
static int
AddClassListEntriesToMixinsOfSet(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                                 Tcl_Obj *resultSet, NsfCmdList *mixinOfs, int appendResult,
                                 const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int GetAllClassMixinsOf(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                    Tcl_Obj *resultSet, /*@notnull@*/ NsfClass *startCl,
                    int isMixin, int appendResult,
                    const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
AddClassListEntriesToMixinsOfSet(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                                 Tcl_Obj *resultSet, NsfCmdList *mixinOfs, int appendResult,
                                 const char *pattern, NsfObject *matchObject) {
  NsfCmdList *m;

  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(resultSet != NULL);
  nonnull_assert(mixinOfs != NULL);

  for (m = mixinOfs; m; m = m->nextPtr) {
    NsfClass *cl;
    int rc, new;

    /* We must not have deleted commands in the list */
    assert((Tcl_Command_flags(m->cmdPtr) & CMD_IS_DELETED) == 0);

    cl = NsfGetClassFromCmdPtr(m->cmdPtr);
    assert(cl != NULL);

    rc = AddToResultSet(interp, destTablePtr, resultSet,
                        &cl->object, &new,
                        appendResult, pattern, matchObject);
    if (rc != 0) {return 1;}
    if (new != 0) {
      /*fprintf(stderr, "... new mixin -closure of %s => %s\n",
        ClassName(startCl), ClassName(cl));*/
      rc = GetAllClassMixinsOf(interp, destTablePtr, resultSet, cl, 1,
                               appendResult, pattern, matchObject);
      if (rc != 0) {return 1;}
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * GetAllClassMixinsOf --
 *
 *    Computes a set of classes, into which this class was mixed in
 *    via as a class mixin. The function gets recursively all per
 *    class mixins from an class and its subclasses and adds it
 *    into an initialized object ptr hash-table (TCL_ONE_WORD_KEYS)
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The set of classes is returned in the provided hash-table
 *
 *----------------------------------------------------------------------
 */

static int
GetAllClassMixinsOf(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                    Tcl_Obj *resultSet, /*@notnull@*/ NsfClass *startCl,
                    int isPCM, int appendResult,
                    const char *pattern, NsfObject *matchObject) {
  int rc = 0, new = 0;
  NsfClasses *sc;

  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(resultSet != NULL);
  nonnull_assert(startCl != NULL);

  /*fprintf(stderr, "GetAllClassMixinsOf startCl = %p %s, opt %p, isPCM %d\n",
    startCl, ClassName(startCl), startCl->opt, isPCM);*/

  /*
   * If the startCl is a per class mixin, add it to the result set
   */
  if (isPCM != 0) {
    rc = AddToResultSet(interp, destTablePtr, resultSet,
                        &startCl->object, &new,
                        appendResult, pattern, matchObject);
    if (rc == 1) {return rc;}

    /*
     * check all subclasses of startCl for mixins
     */
    for (sc = startCl->sub; sc; sc = sc->nextPtr) {
#if !defined(NDEBUG)
      if (sc->cl == startCl) {
        /*
         * Sanity check: it seems that we can create via
         *  __default_superclass a class which has itself as subclass!
         */
        fprintf(stderr, "... STRANGE %p is subclass of %p %s, sub %p\n",
                (void *)sc->cl, (void *)startCl, ClassName(startCl), (void *)startCl->sub);
        continue;
      }
#endif
      assert(sc->cl != startCl);
      rc = GetAllClassMixinsOf(interp, destTablePtr, resultSet,
                               sc->cl, isPCM,
                               appendResult, pattern, matchObject);
      if (rc != 0) {
        return rc;
      }
    }
  }

  /*
   * Check, if startCl has a subclass which is a per-class mixin of some other
   * class(es)
   */
  {
    NsfClasses *subClasses = TransitiveSubClasses(startCl), *subClass;

    for (subClass = subClasses; subClass; subClass = subClass->nextPtr) {
      NsfClass *subCl = subClass->cl;

      /*fprintf(stderr, "... check subclass = %p %s, opt %p, isPCM %d\n",
        subCl, ClassName(subCl), subCl->opt, isPCM);*/

      if (subCl->opt != NULL && subCl->opt->isClassMixinOf) {
        rc = AddClassListEntriesToMixinsOfSet(interp, destTablePtr, resultSet,
                                              subCl->opt->isClassMixinOf,
                                              appendResult, pattern, matchObject);
        if (rc != 0) {goto subclassExit;}
      }
    }

  subclassExit:
    if (subClasses != NULL) {
      NsfClassListFree(subClasses);
    }
    if (rc != 0) {
      return rc;
    }
  }

  /*
   * Check, if startCl is a per-class mixin of some other classes
   */
  if (startCl->opt != NULL && startCl->opt->isClassMixinOf) {
    rc = AddClassListEntriesToMixinsOfSet(interp, destTablePtr, resultSet,
                                          startCl->opt->isClassMixinOf,
                                          appendResult, pattern, matchObject);
  }

  return rc;
}

/*
 *----------------------------------------------------------------------
 * GetAllClassMixins --
 *
 *    Computes a set class-mixins of a given class and handles
 *    transitive cases. The classes are added it into an initialized
 *    object ptr hash-table (TCL_ONE_WORD_KEYS)
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The set of classes is returned in the provided hash-table
 *
 *----------------------------------------------------------------------
 */

static int GetAllClassMixins(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                  Tcl_Obj *resultObj, NsfClass *startCl,
                  int withGuards, const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
GetAllClassMixins(Tcl_Interp *interp, Tcl_HashTable *destTablePtr,
                  Tcl_Obj *resultObj, NsfClass *startCl,
                  int withGuards, const char *pattern, NsfObject *matchObject) {
  int rc = 0, new = 0;
  NsfClass *cl;
  NsfClasses *sc;

  nonnull_assert(interp != NULL);
  nonnull_assert(destTablePtr != NULL);
  nonnull_assert(resultObj != NULL);
  nonnull_assert(startCl != NULL);

  /*
   * check this class for class mixins.
   */
  if (startCl->opt != NULL) {
    NsfCmdList *m;

    for (m = startCl->opt->classMixins; m; m = m->nextPtr) {

      /* we should have no deleted commands in the list */
      assert((Tcl_Command_flags(m->cmdPtr) & CMD_IS_DELETED) == 0);

      cl = NsfGetClassFromCmdPtr(m->cmdPtr);
      assert(cl != NULL);

      /* fprintf(stderr, "class mixin found: %s\n", ClassName(cl)); */

      if ((withGuards) && (m->clientData)) {
        /* fprintf(stderr, "AddToResultSetWithGuards: %s\n", ClassName(cl)); */
        rc = AddToResultSetWithGuards(interp, destTablePtr, resultObj,
                                      cl, m->clientData, &new,
                                      1, pattern, matchObject);
      } else {
        /* fprintf(stderr, "AddToResultSet: %s\n", ClassName(cl)); */
        rc = AddToResultSet(interp, destTablePtr, resultObj,
                            &cl->object, &new,
                            1, pattern, matchObject);
      }
      if (rc == 1) {return rc;}

      if (new != 0) {
        /* fprintf(stderr, "class mixin GetAllClassMixins for: %s (%s)\n",
           ClassName(cl), ClassName(startCl)); */
        rc = GetAllClassMixins(interp, destTablePtr, resultObj,
                               cl, withGuards,
                               pattern, matchObject);
        if (rc != 0) {return rc;}
      }
    }
  }


  /*
   * Check all superClasses of startCl for class mixins.
   */
  for (sc = startCl->super; sc; sc = sc->nextPtr) {
    /* fprintf(stderr, "Superclass GetAllClassMixins for %s (%s)\n",
       ClassName(sc->cl), ClassName(startCl)); */
    rc = GetAllClassMixins(interp, destTablePtr, resultObj,
                           sc->cl, withGuards,
                           pattern, matchObject);
    if (rc != 0) {return rc;}
  }
  return rc;
}

/*
 *----------------------------------------------------------------------
 * RemoveFromClassMixinsOf --
 *
 *    Remove the class (provided as a cmd) from all isClassMixinOf definitions
 *    from the provided classes (provided as cmdlist).
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially some entries in the isClassMixinOf lists.
 *
 *----------------------------------------------------------------------
 */

static void RemoveFromClassMixinsOf(Tcl_Command cmd, NsfCmdList *cmdlist) nonnull(1) nonnull(2);

static void
RemoveFromClassMixinsOf(Tcl_Command cmd, NsfCmdList *cmdList) {

  nonnull_assert(cmd != NULL);
  nonnull_assert(cmdList != NULL);

  do {
    NsfClass *ncl = NsfGetClassFromCmdPtr(cmdList->cmdPtr);
    NsfClassOpt *nclopt = (ncl != NULL) ? ncl->opt : NULL;
    if (nclopt != NULL) {
      NsfCmdList *del = CmdListFindCmdInList(cmd, nclopt->isClassMixinOf);
      if (del != NULL) {
        /* fprintf(stderr, "Removing class %s from isClassMixinOf of class %s\n",
           ClassName(cl), ObjStr(NsfGetClassFromCmdPtr(cmdList->cmdPtr)->object.cmdName)); */
        del = CmdListRemoveFromList(&nclopt->isClassMixinOf, del);
        CmdListDeleteCmdListEntry(del, GuardDel);
      }
    }
    cmdList = cmdList->nextPtr;
  } while (cmdList != NULL);
}

/*
 *----------------------------------------------------------------------
 * RemoveFromObjectMixinsOf --
 *
 *    Remove the class (provided as a cmd) from all isObjectMixinOf definitions
 *    from the provided classes (provided as cmdList).
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially some entries in the isObjectMixinOf lists.
 *
 *----------------------------------------------------------------------
 */

static void RemoveFromObjectMixinsOf(Tcl_Command cmd, NsfCmdList *cmdList) nonnull(1) nonnull(2);

static void
RemoveFromObjectMixinsOf(Tcl_Command cmd, NsfCmdList *cmdList) {

  nonnull_assert(cmd != NULL);
  nonnull_assert(cmdList != NULL);

  do {
    NsfClass *cl = NsfGetClassFromCmdPtr(cmdList->cmdPtr);
    NsfClassOpt *clopt = (cl != NULL) ? cl->opt : NULL;
    if (clopt != NULL) {
      NsfCmdList *del = CmdListFindCmdInList(cmd, clopt->isObjectMixinOf);
      if (del != NULL) {
        /* fprintf(stderr, "Removing object %s from isObjectMixinOf of Class %s\n",
           ObjectName(object), ObjStr(NsfGetClassFromCmdPtr(cmdList->cmdPtr)->object.cmdName)); */
        del = CmdListRemoveFromList(&clopt->isObjectMixinOf, del);
        CmdListDeleteCmdListEntry(del, GuardDel);
      }
    } /* else fprintf(stderr, "CleanupDestroyObject %s: NULL pointer in mixins!\n", ObjectName(object)); */
    cmdList = cmdList->nextPtr;
  } while(likely(cmdList != NULL));
}

/*
 *----------------------------------------------------------------------
 * RemoveFromClassmixins --
 *
 *    Remove the class (provided as a cmd) from all class mixins lists
 *    from the provided classes (provided as cmdList).
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially some entries in the class mixins lists.
 *
 *----------------------------------------------------------------------
 */

static void RemoveFromClassmixins(Tcl_Command cmd, NsfCmdList *cmdList) nonnull(1) nonnull(2);

static void
RemoveFromClassmixins(Tcl_Command cmd, NsfCmdList *cmdList) {

  nonnull_assert(cmd != NULL);
  nonnull_assert(cmdList != NULL);

  do {
    NsfClass *cl = NsfGetClassFromCmdPtr(cmdList->cmdPtr);
    NsfClassOpt *clopt = (cl != NULL) ? cl->opt : NULL;

    if (clopt != NULL) {
      NsfCmdList *del = CmdListFindCmdInList(cmd, clopt->classMixins);

      if (del != NULL) {
        /* fprintf(stderr, "Removing class %s from mixins of object %s\n",
           ClassName(cl), ObjStr(NsfGetObjectFromCmdPtr(cmdList->cmdPtr)->cmdName)); */
        del = CmdListRemoveFromList(&clopt->classMixins, del);
        CmdListDeleteCmdListEntry(del, GuardDel);
        if (cl->object.mixinOrder != NULL) {
          MixinResetOrder(&cl->object);
        }
      }
    }
    cmdList = cmdList->nextPtr;
  } while (likely(cmdList != NULL));
}

/*
 *----------------------------------------------------------------------
 * RemoveFromObjectMixins --
 *
 *    Remove the class (provided as a cmd) from all object mixin lists
 *    from the provided classes (provided as cmdList).
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially some entries in the object mixins lists.
 *
 *----------------------------------------------------------------------
 */
static void RemoveFromObjectMixins(Tcl_Command cmd, NsfCmdList *cmdList) nonnull(1) nonnull(2);

static void
RemoveFromObjectMixins(Tcl_Command cmd, NsfCmdList *cmdList) {

  nonnull_assert(cmd != NULL);
  nonnull_assert(cmdList != NULL);

  do {
    NsfObject *nobj = NsfGetObjectFromCmdPtr(cmdList->cmdPtr);
    NsfObjectOpt *objopt = (nobj != 0) ? nobj->opt : NULL;

    if (objopt != NULL) {
      NsfCmdList *del = CmdListFindCmdInList(cmd, objopt->objMixins);

      if (del != NULL) {
        /* fprintf(stderr, "Removing class %s from mixins of object %s\n",
           ClassName(cl), ObjStr(NsfGetObjectFromCmdPtr(cmdList->cmdPtr)->cmdName)); */
        del = CmdListRemoveFromList(&objopt->objMixins, del);
        CmdListDeleteCmdListEntry(del, GuardDel);
        if (nobj->mixinOrder != NULL) {
          MixinResetOrder(nobj);
        }
      }
    }
    cmdList = cmdList->nextPtr;
  } while (likely(cmdList != NULL));
}


/*
 *----------------------------------------------------------------------
 * ResetOrderOfObjectsUsingThisClassAsObjectMixin --
 *
 *    Reset the per-object mixin order for all objects having this class as
 *    per-object mixin.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially the mixin list for the objects.
 *
 *----------------------------------------------------------------------
 */
static void ResetOrderOfObjectsUsingThisClassAsObjectMixin(NsfClass *cl) nonnull(1);

static void
ResetOrderOfObjectsUsingThisClassAsObjectMixin(NsfClass *cl) {

  /*fprintf(stderr, "ResetOrderOfObjectsUsingThisClassAsObjectMixin %s - %p\n",
    ClassName(cl), cl->opt);*/

  nonnull_assert(cl != NULL);

  if (cl->opt != NULL) {
    NsfCmdList *ml;

    for (ml = cl->opt->isObjectMixinOf; ml; ml = ml->nextPtr) {
      NsfObject *object = NsfGetObjectFromCmdPtr(ml->cmdPtr);

      if (object != NULL) {
        if (object->mixinOrder != NULL) {
          MixinResetOrder(object);
        }
        object->flags &= ~NSF_MIXIN_ORDER_VALID;
      }
    }
  }
}

/*
 *----------------------------------------------------------------------
 * MixinInvalidateObjOrders --
 *
 *    Reset mixin order for all instances of the class and the instances of
 *    its dependent subclasses. This function is typically called, when the
 *    the class hierarchy or the class mixins have changed and invalidate
 *    mixin entries in all dependent instances.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Deletes potentially the mixin list for the objects and classes.
 *
 *----------------------------------------------------------------------
 */

static void MixinInvalidateObjOrders(Tcl_Interp *interp, NsfClass *cl, NsfClasses *subClasses)
  nonnull(1) nonnull(2) nonnull(3);

static void
MixinInvalidateObjOrders(Tcl_Interp *interp, NsfClass *cl, NsfClasses *subClasses) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(subClasses != NULL);

  /*
   * Iterate over the subclass hierarchy.
   */
  for (; likely(subClasses != NULL); subClasses = subClasses->nextPtr) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry *hPtr;
    Tcl_HashTable *instanceTablePtr;

    /*
     * Reset mixin order for all objects having this class as per object mixin
     */
    ResetOrderOfObjectsUsingThisClassAsObjectMixin(subClasses->cl);

    if (subClasses->cl->parsedParamPtr != NULL) {
      ParsedParamFree(subClasses->cl->parsedParamPtr);
      subClasses->cl->parsedParamPtr = NULL;
    }

    instanceTablePtr = &subClasses->cl->instances;
    for (hPtr = Tcl_FirstHashEntry(instanceTablePtr, &hSrch); hPtr;
         hPtr = Tcl_NextHashEntry(&hSrch)) {
      NsfObject *object = (NsfObject *)Tcl_GetHashKey(instanceTablePtr, hPtr);

      assert(object != NULL);

      if (((object->flags & NSF_DURING_DELETE) == 0u)
          && ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u)) {
        MixinResetOrder(object);
        object->flags &= ~NSF_MIXIN_ORDER_VALID;
      }
    }
  }

}

/*
 *----------------------------------------------------------------------
 * MixinComputeDefined --
 *
 *    This function computes the mixin order for the provided object and
 *    adjusts the mixin flags accordingly. The mixin order is either
 *
 *       DEFINED (there are mixins on the instance),
 *       NONE    (there are no mixins for the instance),
 *       or INVALID (a class restructuring has occurred,
 *               thus it is not clear whether mixins are defined or not).
 *
 *    If the mixin order is INVALID, MixinComputeDefined can be used to
 *    compute the order and set the instance to DEFINED or NONE
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Might alter the mixin order.
 *
 *----------------------------------------------------------------------
 */
static void
MixinComputeDefined(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  MixinComputeOrder(interp, object);
  object->flags |= NSF_MIXIN_ORDER_VALID;
  if (object->mixinOrder != NULL) {
    object->flags |= NSF_MIXIN_ORDER_DEFINED;
  } else {
    object->flags &= ~NSF_MIXIN_ORDER_DEFINED;
  }
}

/*
 *----------------------------------------------------------------------
 * ComputePrecedenceList --
 *
 *    Returns the precedence list for the provided object.  The precedence
 *    list can optionally include the mixins and the root class. If pattern is
 *    provided, this is used as well for filtering. The caller has to free the
 *    resulting list via NsfClassListFree();
 *
 * Results:
 *    Precedence list in form of a class list, potentially NULL due to filtering.
 *
 * Side effects:
 *    Allocated class list.
 *
 *----------------------------------------------------------------------
 */

static NsfClasses *ComputePrecedenceList(Tcl_Interp *interp, NsfObject *object,
                                         const char *pattern, int withMixins, int withRootClass)
  nonnull(1) nonnull(2);

static NsfClasses *
ComputePrecedenceList(Tcl_Interp *interp, NsfObject *object,
                      const char *pattern,
                      int withMixins, int withRootClass) {
  NsfClasses *precedenceList = NULL, *pcl, **npl = &precedenceList;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (withMixins != 0) {
    if ((object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
      MixinComputeDefined(interp, object);
    }
    if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
      NsfCmdList *ml;

      for (ml = object->mixinOrder; ml; ml = ml->nextPtr) {
        NsfClass *mixin = NsfGetClassFromCmdPtr(ml->cmdPtr);

        if (pattern && !Tcl_StringMatch(ClassName(mixin), pattern)) {
          continue;
        }
        npl = NsfClassListAdd(npl, mixin, NULL);
      }
    }
  }

  pcl = PrecedenceOrder(object->cl);
  for (; pcl; pcl = pcl->nextPtr) {
    if (withRootClass == 0 && IsRootClass(pcl->cl)) {
      continue;
    }
    if (pattern && !Tcl_StringMatch(ClassName(pcl->cl), pattern)) {
      continue;
    }
    npl = NsfClassListAdd(npl, pcl->cl, NULL);
  }
  return precedenceList;
}

/*
 *----------------------------------------------------------------------
 * SeekCurrent --
 *
 *    Walk through the command list until the provided command is reached.
 *    return the next entry. If the provided cmd is NULL, then return the
 *    first entry.
 *
 * Results:
 *    Command list pointer or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfCmdList *SeekCurrent(Tcl_Command cmd, register NsfCmdList *cmdListPtr) nonnull(2);

static NsfCmdList *
SeekCurrent(Tcl_Command cmd, register NsfCmdList *cmdListPtr) {

  nonnull_assert(cmdListPtr != NULL);

  if (cmd != NULL) {
    for (; likely(cmdListPtr != NULL); cmdListPtr = cmdListPtr->nextPtr) {
      if (cmdListPtr->cmdPtr == cmd) {
        return cmdListPtr->nextPtr;
      }
    }
    return NULL;
  }
  return cmdListPtr;
}

/*
 *----------------------------------------------------------------------
 * CanInvokeMixinMethod --
 *
 *    Check, whether the provided cmd is allowed to be dispatch in a mixin.
 *
 * Results:
 *    Tcl result code or NSF_CHECK_FAILED in case, search should continue
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int CanInvokeMixinMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Command cmd, NsfCmdList *cmdList)
  nonnull(1) nonnull(2) nonnull(4);

static int
CanInvokeMixinMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Command cmd, NsfCmdList *cmdList) {
  int result = TCL_OK;
  int cmdFlags = Tcl_Command_flags(cmd);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(cmdList != NULL);

  if ((cmdFlags & NSF_CMD_CALL_PRIVATE_METHOD) ||
      ((cmdFlags & NSF_CMD_CLASS_ONLY_METHOD) && !NsfObjectIsClass(object))) {
    /*
     * The command is not applicable for objects (i.e. might crash,
     * since it expects a class record); therefore skip it
     */
    return NSF_CHECK_FAILED;
  }

  if (cmdList->clientData && !RUNTIME_STATE(interp)->guardCount) {
    /*fprintf(stderr, "guard call\n");*/
    result = GuardCall(object, interp, (Tcl_Obj *)cmdList->clientData, NULL);
  }

  return result;
}


/*
 *----------------------------------------------------------------------
 * MixinSearchProc --
 *
 *    Search for a method name in the mixin list of the provided
 *    object. Depending on the state of the mixin stack, the search starts
 *    at the beginning or at the last dispatched, shadowed method on
 *    the mixin path.
 *
 * Results:
 *    Tcl result code.
 *    Returns as well always cmd (maybe NULL) in cmdPtr.
 *    Returns on success as well the class and the currentCmdPointer
 *    for continuation in next.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int MixinSearchProc(Tcl_Interp *interp, NsfObject *object,
                const char *methodName, Tcl_Obj *methodObj,
                NsfClass **clPtr, Tcl_Command *currentCmdPtr, Tcl_Command *cmdPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(5) nonnull(6) nonnull(7);

static int
MixinSearchProc(Tcl_Interp *interp, NsfObject *object,
                const char *methodName, Tcl_Obj *methodObj,
                NsfClass **clPtr, Tcl_Command *currentCmdPtr, Tcl_Command *cmdPtr) {
  Tcl_Command cmd = NULL;
  NsfCmdList *cmdList;
  NsfClass *cl = NULL;
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(clPtr != NULL);
  nonnull_assert(currentCmdPtr != NULL);
  nonnull_assert(cmdPtr != NULL);
  
  assert(object->mixinStack != NULL);

  /* ensure that the mixin order is valid */
  assert((object->flags & NSF_MIXIN_ORDER_VALID) != 0u);

  if (object->mixinOrder == NULL) {
    return TCL_OK;
  }

  cmdList = SeekCurrent(object->mixinStack->currentCmdPtr, object->mixinOrder);
  RUNTIME_STATE(interp)->currentMixinCmdPtr = (cmdList != NULL) ? cmdList->cmdPtr : NULL;

  /*fprintf(stderr,"searching for '%s' in %p\n", methodName, cmdList);
  CmdListPrint(interp, "MixinSearch CL = \n", cmdList);*/

  if (unlikely((*clPtr != NULL) && (*cmdPtr != NULL))) {
    Tcl_Command lastCmdPtr = NULL;

    /*fprintf(stderr, "... new branch\n");*/

    for (; cmdList; cmdList = cmdList->nextPtr) {
      NsfClass *cl1;

      /*
       * Ignore deleted commands
       */
      if (Tcl_Command_flags(cmdList->cmdPtr) & CMD_IS_DELETED) {
        continue;
      }

      cl1 = NsfGetClassFromCmdPtr(cmdList->cmdPtr);
      assert(cl1 != NULL);
      lastCmdPtr = cmdList->cmdPtr;

      if (cl1 == *clPtr) {
        /*
         * The wanted class was found. Check guards and permissions to
         * determine whether we can invoke this method.
         */
        result = CanInvokeMixinMethod(interp, object, *cmdPtr, cmdList);

        if (likely(result == TCL_OK)) {
          cl = cl1;
        } else if (result == NSF_CHECK_FAILED) {
          result = TCL_OK;
        }
        /*
         * No matter, what the result is, stop the search through the mixin
         * classes here.
         */
        break;
      }
    }

    if (cl != NULL) {
      assert(cmdList != NULL);
      /*
       * On success: return class and cmdList->cmdPtr;
       */
      *currentCmdPtr = cmdList->cmdPtr;
      /*fprintf(stderr, "... mixinsearch success returns %p (cl %s)\n", cmd, ClassName(cl));*/

    } else {
      /*
       * We did not find the absolute entry in the mixins.  Set the
       * currentCmdPtr (on the mixin stack) to the last entry to flag, that
       * the mixin list should not started again on a next.
       */
      *cmdPtr = NULL;
      *currentCmdPtr = lastCmdPtr;
      /*fprintf(stderr, "... mixinsearch success failure %p (cl %s)\n", cmd, ClassName(cl));*/
    }

    return result;

  } else {

    for (; cmdList; cmdList = cmdList->nextPtr) {
      /*
       * Ignore deleted commands
       */
      if (Tcl_Command_flags(cmdList->cmdPtr) & CMD_IS_DELETED) {
        continue;
      }
      cl = NsfGetClassFromCmdPtr(cmdList->cmdPtr);
      assert(cl != NULL);
      /*
        fprintf(stderr, "+++ MixinSearch %s->%s in %p cmdPtr %p clientData %p\n",
        ObjectName(object), methodName, cmdList,
        cmdList->cmdPtr, cmdList->clientData);
      */
      cmd = FindMethod(cl->nsPtr, methodName);
      if (cmd == NULL) {
        continue;
      }

      result = CanInvokeMixinMethod(interp, object, cmd, cmdList);

      if (unlikely(result == TCL_ERROR)) {
        return result;
      } else if (result == NSF_CHECK_FAILED) {
        result = TCL_OK;
        cmd = NULL;
        continue;
      }

      /*
       * cmd was found and is applicable. We return class and cmdPtr.
       */
      *clPtr = cl;
      *currentCmdPtr = cmdList->cmdPtr;
      /*fprintf(stderr, "mixinsearch returns %p (cl %s)\n", cmd, ClassName(cl));*/
      break;
    }

  }
  *cmdPtr = cmd;
  return result;
}

/*
 * info option for mixins and class mixins
 */
static int MixinInfo(Tcl_Interp *interp, NsfCmdList *m, const char *pattern,
                     int withGuards, NsfObject *matchObject)
  nonnull(1);

static int
MixinInfo(Tcl_Interp *interp, NsfCmdList *m, const char *pattern,
          int withGuards, NsfObject *matchObject) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);

  /*fprintf(stderr, "   mixin info m=%p, pattern %s, matchObject %p\n",
    m, pattern, matchObject);*/

  while (m) {
    NsfClass *mixinClass = NsfGetClassFromCmdPtr(m->cmdPtr);

    /* fprintf(stderr, "   mixin info m=%p, next=%p, pattern %s, matchObject %p\n",
       m, m->next, pattern, matchObject);*/

    if (mixinClass != NULL &&
        (pattern == NULL
         || (matchObject != NULL && &(mixinClass->object) == matchObject)
         || (matchObject == NULL && Tcl_StringMatch(ObjStr(mixinClass->object.cmdName), pattern)))) {
      if (withGuards && m->clientData) {
        Tcl_Obj *l = Tcl_NewListObj(0, NULL);
        Tcl_Obj *g = (Tcl_Obj *) m->clientData;
        Tcl_ListObjAppendElement(interp, l, mixinClass->object.cmdName);
        Tcl_ListObjAppendElement(interp, l, NsfGlobalObjs[NSF_GUARD_OPTION]);
        Tcl_ListObjAppendElement(interp, l, g);
        Tcl_ListObjAppendElement(interp, list, l);
      } else {
        Tcl_ListObjAppendElement(interp, list, mixinClass->object.cmdName);
      }
      if (matchObject != NULL) {
        break;
      }
    }
    m = m->nextPtr;
  }

  Tcl_SetObjResult(interp, list);
  return TCL_OK;
}

/*
 * info option for mixinofs and isClassMixinOf
 */

static Tcl_Command MixinSearchMethodByName(NsfCmdList *mixinList, const char *name, NsfClass **clPtr)
  nonnull(1) nonnull(2) nonnull(3);

static Tcl_Command
MixinSearchMethodByName(NsfCmdList *mixinList, const char *name, NsfClass **clPtr) {
  Tcl_Command cmd;

  nonnull_assert(mixinList != NULL);
  nonnull_assert(name != NULL);
  nonnull_assert(clPtr != NULL);

  for (; likely(mixinList != NULL);  mixinList = mixinList->nextPtr) {
    NsfClass *foundCl = NsfGetClassFromCmdPtr(mixinList->cmdPtr);

    if (foundCl && SearchCMethod(foundCl, name, &cmd)) {
      *clPtr = foundCl;
      return cmd;
    }
  }
  return NULL;
}


/*
 *  Filter-Commands
 */

/*
 * The search method implements filter search order for object and
 * class filter: first a given name is interpreted as fully qualified
 * method name. If no method is found, a proc is searched with fully
 * name. Otherwise the simple name is searched on the heritage order:
 * object (only for per-object filters), class, meta-class
 */

static Tcl_Command FilterSearch(const char *name, NsfObject *startingObject,
             NsfClass *startingClass, NsfClass **clPtr)
  nonnull(1) nonnull(4);

static Tcl_Command
FilterSearch(const char *name, NsfObject *startingObject,
             NsfClass *startingClass, NsfClass **clPtr) {
  Tcl_Command cmd = NULL;

  nonnull_assert(name != NULL);
  nonnull_assert(clPtr != NULL);

  if (startingObject != NULL) {
    NsfObjectOpt *opt = startingObject->opt;
    /*
     * the object-specific filter can also be defined on the object's
     * class, its hierarchy, or the respective class mixins; thus use the
     * object's class as start point for the class-specific search then ...
     */
    startingClass = startingObject->cl;

    /*
     * search for filters on object mixins
     */
    if (opt != NULL && opt->objMixins != NULL) {
      if ((cmd = MixinSearchMethodByName(opt->objMixins, name, clPtr))) {
        return cmd;
      }
    }
  }

  /*
   * search for class filters on class mixins
   */
  if (startingClass != NULL) {
    NsfClassOpt *opt = startingClass->opt;
    if (opt != NULL && opt->classMixins != NULL) {
      if ((cmd = MixinSearchMethodByName(opt->classMixins, name, clPtr))) {
        return cmd;
      }
    }
  }

  /*
   * search for object procs that are used as filters
   */
  if (startingObject != NULL && startingObject->nsPtr) {
    /*fprintf(stderr, "search filter %s as proc \n", name);*/
    if ((cmd = FindMethod(startingObject->nsPtr, name))) {
      *clPtr = (NsfClass *)startingObject;
      return cmd;
    }
  }

  /*
   * ok, no filter on obj or mixins -> search class
   */
  if (startingClass != NULL) {
    *clPtr = SearchCMethod(startingClass, name, &cmd);
    if (*clPtr == NULL) {
      /*
       * If no filter is found yet -> search the meta-class
       */
      *clPtr = SearchCMethod(startingClass->object.cl, name, &cmd);
    }
  }
  return cmd;
}

/*
 * Filter Guards
 */

/* check a filter guard, return 1 if ok */
static int GuardCheck(Tcl_Interp *interp, Tcl_Obj *guardObj) nonnull(1) nonnull(2);

static int
GuardCheck(Tcl_Interp *interp, Tcl_Obj *guardObj) {
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(guardObj != NULL);

  /*
   * if there are more than one filter guard for this filter
   * (i.e. they are inherited), then they are OR combined
   * -> if one check succeeds => return 1
   */

  /*fprintf(stderr, "checking guard **%s**\n", ObjStr(guardObj));*/

  rst->guardCount++;
  result = CheckConditionInScope(interp, guardObj);
  rst->guardCount--;

  /*fprintf(stderr, "checking guard **%s** returned rc=%d\n", ObjStr(guardObj), rc);*/

  if (likely(result == TCL_OK)) {
    /* fprintf(stderr, " +++ OK\n"); */
    return TCL_OK;

  } else if (unlikely(result == TCL_ERROR)) {
    Tcl_Obj *sr = Tcl_GetObjResult(interp);

    INCR_REF_COUNT(sr);
    NsfPrintError(interp, "Guard error: '%s'\n%s", ObjStr(guardObj), ObjStr(sr));
    DECR_REF_COUNT(sr);
    return TCL_ERROR;
  }

  /*
    fprintf(stderr, " +++ FAILED\n");
  */
  return NSF_CHECK_FAILED;
}

/*
  static void
  GuardPrint(Tcl_Interp *interp, ClientData clientData) {
  Tcl_Obj *guardObj = (TclObj *) clientData;
  fprintf(stderr, " +++ <GUARDS> \n");
  if (guardObj != NULL) {
  fprintf(stderr, "   *     %s \n", ObjStr(guardObj));
  }
  fprintf(stderr, " +++ </GUARDS>\n");
  }
*/

static void
GuardDel(NsfCmdList *guardList) {

  nonnull_assert(guardList != NULL);

  /*fprintf(stderr, "GuardDel %p clientData = %p\n",
    guardList, (guardList != NULL) ? guardList->clientData : NULL);*/

  if (guardList->clientData != NULL) {
    DECR_REF_COUNT2("guardObj", (Tcl_Obj *)guardList->clientData);
    guardList->clientData = NULL;
  }
}

NSF_INLINE static void
GuardAdd(NsfCmdList *guardList, Tcl_Obj *guardObj) {

  nonnull_assert(guardList != NULL);
  nonnull_assert(guardObj != NULL);

  GuardDel(guardList);
  if (strlen(ObjStr(guardObj)) > 0) {
    INCR_REF_COUNT2("guardObj", guardObj);
    guardList->clientData = guardObj;
    /*fprintf(stderr, "guard added to %p cmdPtr=%p, clientData= %p\n",
      guardList, guardList->cmdPtr, guardList->clientData);
    */
  }
}

static int
GuardCall(NsfObject *object, Tcl_Interp *interp, Tcl_Obj *guardObj, NsfCallStackContent *cscPtr) {
  int result = TCL_OK;
  Tcl_Obj *res = Tcl_GetObjResult(interp); /* save the result */
  CallFrame frame, *framePtr = &frame;

  nonnull_assert(object != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(guardObj != NULL);

  INCR_REF_COUNT(res);

  /*
   * For the guard push a fake call-frame on the Tcl stack so that
   * e.g. a "self calledproc" and other methods in the guard behave
   * like in the proc.
   */
  if (cscPtr != NULL) {
    Nsf_PushFrameCsc(interp, cscPtr, framePtr);
  } else {
    Nsf_PushFrameObj(interp, object, framePtr);
  }
  result = GuardCheck(interp, guardObj);

  if (cscPtr != NULL) {
    Nsf_PopFrameCsc(interp, framePtr);
  } else {
    Nsf_PopFrameObj(interp, framePtr);
  }

  if (result != TCL_ERROR) {
    Tcl_SetObjResult(interp, res);  /* restore the result */
  }

  DECR_REF_COUNT(res);
  return result;
}

/*
 *----------------------------------------------------------------------
 * GuardAddFromDefinitionList --
 *
 *    Add a guard to the specified destination list (first arg) from a list of
 *    definitions (last arg).  If the provided cmd is found in the list of
 *    definitions, it is added to the destination list if it has non-null
 *    client data.
 *
 * Results:
 *    Returns 0 or 1 depending on wether the cmd is part of the
 *    definition list.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int GuardAddFromDefinitionList(NsfCmdList *dest, Tcl_Command interceptorCmd,
                                      NsfCmdList *interceptorDefList)
  nonnull(1) nonnull(2) nonnull(3);

static int
GuardAddFromDefinitionList(NsfCmdList *dest, Tcl_Command interceptorCmd,
                           NsfCmdList *interceptorDefList) {
  NsfCmdList *h;

  nonnull_assert(interceptorCmd != NULL);
  nonnull_assert(dest != NULL);
  nonnull_assert(interceptorDefList != NULL);

  h = CmdListFindCmdInList(interceptorCmd, interceptorDefList);
  if (h != NULL) {
    if (h->clientData != NULL) {
      GuardAdd(dest, (Tcl_Obj *) h->clientData);
    }
    return 1;
  }

  return 0;
}

/*
 *----------------------------------------------------------------------
 * GuardAddInheritedGuards --
 *
 *    Add a inherited guards to the provided destination list.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Updates potentially destination list
 *
 *----------------------------------------------------------------------
 */
static void GuardAddInheritedGuards(Tcl_Interp *interp, NsfCmdList *dest,
                                    NsfObject *object, Tcl_Command filterCmd)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
GuardAddInheritedGuards(Tcl_Interp *interp, NsfCmdList *dest,
                        NsfObject *object, Tcl_Command filterCmd) {
  NsfClasses *pl;
  int guardAdded = 0;
  NsfObjectOpt *opt;

  nonnull_assert(filterCmd != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(dest != NULL);
  nonnull_assert(object != NULL);

  /* search guards for class filters registered on mixins */
  if (((object->flags & NSF_MIXIN_ORDER_VALID)) == 0u) {
    MixinComputeDefined(interp, object);
  }
  if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
    NsfCmdList *ml;

    for (ml = object->mixinOrder; ml != NULL && !guardAdded; ml = ml->nextPtr) {
      NsfClass *mixin = NsfGetClassFromCmdPtr(ml->cmdPtr);

      if (mixin != NULL && mixin->opt != NULL && mixin->opt->classFilters != NULL) {
        guardAdded = GuardAddFromDefinitionList(dest, filterCmd, mixin->opt->classFilters);
      }
    }
  }

  /* search per-object filters */
  opt = object->opt;
  if (!guardAdded && opt != NULL && opt->objFilters != NULL) {
    guardAdded = GuardAddFromDefinitionList(dest, filterCmd, opt->objFilters);
  }

  if (guardAdded == 0) {
    /* search per-class filters */
    for (pl = PrecedenceOrder(object->cl); !guardAdded && pl; pl = pl->nextPtr) {
      NsfClassOpt *clopt = pl->cl->opt;
      if (clopt != NULL && clopt->classFilters != NULL) {
        guardAdded = GuardAddFromDefinitionList(dest, filterCmd, clopt->classFilters);
      }
    }

    /*
     * if this is not a registered filter, it is an inherited filter, like:
     *   Class create A
     *   A method f ...
     *   Class create B -superclass A
     *   B method {{f {<guard>}}}
     *   B filter f
     * -> get the guard from the filter that inherits it (here B->f)
     */
    if (guardAdded == 0) {
      NsfCmdList *registeredFilter =
        CmdListFindNameInList(interp, (char *) Tcl_GetCommandName(interp, filterCmd),
                              object->filterOrder);
      if (registeredFilter && registeredFilter->clientData) {
        GuardAdd(dest, (Tcl_Obj *) registeredFilter->clientData);
      }
    }
  }
}

/*
 *----------------------------------------------------------------------
 * GuardList --
 *
 *    Set interp result to a named guard in the provided guardList. guardList
 *    might be NULL.
 *
 * Results:
 *    interp result
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int GuardList(Tcl_Interp *interp, NsfCmdList *guardList, const char *interceptorName)
  nonnull(1) nonnull(3);

static int
GuardList(Tcl_Interp *interp, NsfCmdList *guardList, const char *interceptorName) {

  nonnull_assert(interp != NULL);
  nonnull_assert(interceptorName != NULL);

  if (guardList != NULL) {
    /* try to find simple name first */
    NsfCmdList *h = CmdListFindNameInList(interp, interceptorName, guardList);
    if (h == NULL) {
      /* maybe it is a qualified name */
      Tcl_Command cmd = NSFindCommand(interp, interceptorName);
      if (cmd != NULL) {
        h = CmdListFindCmdInList(cmd, guardList);
      }
    }
    if (h != NULL) {
      Tcl_ResetResult(interp);
      if (h->clientData != NULL) {
        Tcl_Obj *g = (Tcl_Obj *) h->clientData;
        Tcl_SetObjResult(interp, g);
      }
      return TCL_OK;
    }
  }
  return NsfPrintError(interp, "info guard: can't find filter/mixin %s", interceptorName);
}

/*
 *----------------------------------------------------------------------
 * FilterAddActive --
 *
 *    Add a method name to the set of methods, which were used as filters in
 *    the current interp.
 *
 *    TODO: let the set shrink, when filters are removed.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Adding or updating of a hash entry
 *
 *----------------------------------------------------------------------
 */
static void FilterAddActive(Tcl_Interp *interp, const char *methodName) nonnull(1) nonnull(2);

static void
FilterAddActive(Tcl_Interp *interp, const char *methodName) {
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  Tcl_HashEntry *hPtr;
  int newItem;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);

  hPtr = Tcl_CreateHashEntry(&rst->activeFilterTablePtr, methodName, &newItem);
  if (newItem != 0) {
    Tcl_SetHashValue(hPtr, INT2PTR(1));
  } else {
    int count = PTR2INT(Tcl_GetHashValue(hPtr));
    Tcl_SetHashValue(hPtr, INT2PTR(count+1));
  }
}

/*
 *----------------------------------------------------------------------
 * FilterIsActive --
 *
 *    Check, wether a method name is in the set of methods, which were used as
 *    filters in the current interp.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int FilterIsActive(Tcl_Interp *interp, const char *methodName) nonnull(1) nonnull(2);

static int
FilterIsActive(Tcl_Interp *interp, const char *methodName) {
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  Tcl_HashEntry *hPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);

  hPtr = Tcl_CreateHashEntry(&rst->activeFilterTablePtr, methodName, NULL);
  return (hPtr != NULL);
}

/*
 *----------------------------------------------------------------------
 * FiltersDefined --
 *
 *    Return the number of defined distinct names of filters.
 *
 * Results:
 *    Positive number.
 *
 * Side effects:
 *    none.
 *
 *----------------------------------------------------------------------
 */
static int FiltersDefined(Tcl_Interp *interp) nonnull(1);

static int
FiltersDefined(Tcl_Interp *interp) {
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  nonnull_assert(interp != NULL);

  return Tcl_HashSize(&rst->activeFilterTablePtr);
}


/*
 *----------------------------------------------------------------------
 * FilterAdd --
 *
 *    Append a filter command to the 'filterList' of an obj/class
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    Modifies interp result in error situations.
 *
 *----------------------------------------------------------------------
 */
static int FilterAdd(Tcl_Interp *interp, NsfCmdList **filterList, Tcl_Obj *filterregObj,
                     NsfObject *startingObject, NsfClass *startingClass)
  nonnull(1) nonnull(2) nonnull(3);

static int
FilterAdd(Tcl_Interp *interp, NsfCmdList **filterList, Tcl_Obj *filterregObj,
          NsfObject *startingObject, NsfClass *startingClass) {
  Tcl_Obj *filterObj = NULL;
  Tcl_Obj *guardObj = NULL;
  Tcl_Command cmd;
  NsfCmdList *new;
  NsfClass *cl;

  nonnull_assert(interp != NULL);
  nonnull_assert(filterList != NULL);
  nonnull_assert(filterregObj != NULL);

  /*
   * When the provided nameObj is of type NsfFilterregObjType, the nsf specific
   * converter was called already; otherwise call the converter here.
   */
  if (filterregObj->typePtr != &NsfFilterregObjType) {
    /*fprintf(stderr, "FilterAdd: convert %s in FilterAdd\n", ObjStr(filterregObj));*/
    if (Tcl_ConvertToType(interp, filterregObj, &NsfFilterregObjType) != TCL_OK) {
      return TCL_ERROR;
    }
  } else {
    /*fprintf(stderr, "FilterAdd: %s already converted\n", ObjStr(filterregObj));*/
  }

  NsfFilterregGet(interp, filterregObj, &filterObj, &guardObj);

  if (!(cmd = FilterSearch(ObjStr(filterObj), startingObject, startingClass, &cl))) {
    if (startingObject != NULL) {
      return NsfPrintError(interp, "object filter: can't find filterproc '%s' on %s ",
                           ObjStr(filterObj), ObjectName(startingObject));
    } else {
      return NsfPrintError(interp, "class filter: can't find filterproc '%s' on %s ",
                           ObjStr(filterObj), ClassName(startingClass));
    }
  }

  /*fprintf(stderr, " +++ adding filter %s cl %p\n", ObjStr(nameObj), cl);*/

  new = CmdListAdd(filterList, cmd, cl, /*noDuplicates*/ 1, 1);
  FilterAddActive(interp, ObjStr(filterObj));

  if (guardObj != NULL) {
    GuardAdd(new, guardObj);
  } else if (new->clientData != NULL) {
    GuardDel(new);
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * FilterResetOrder --
 *
 *    Reset the filter order cached in obj->filterOrder
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void FilterResetOrder(NsfObject *object) nonnull(1);

static void
FilterResetOrder(NsfObject *object) {

  nonnull_assert(object != NULL);

  CmdListFree(&object->filterOrder, GuardDel);
  object->filterOrder = NULL;
}

/*
 *----------------------------------------------------------------------
 * FilterSearchAgain --
 *
 *    Search the filter in the hierarchy again with FilterSearch, e.g.  upon
 *    changes in the class hierarchy or mixins that carry the filter command,
 *    so that we can be sure it is still reachable.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static void FilterSearchAgain(Tcl_Interp *interp, NsfCmdList **filters,
                              NsfObject *startingObject, NsfClass *startingClass)
  nonnull(1) nonnull(2);

static void
FilterSearchAgain(Tcl_Interp *interp, NsfCmdList **filters,
                  NsfObject *startingObject, NsfClass *startingClass) {
  NsfCmdList *cmdList, *del;

  nonnull_assert(interp != NULL);
  nonnull_assert(filters != NULL);

  CmdListRemoveDeleted(filters, GuardDel);
  for (cmdList = *filters; cmdList; cmdList = cmdList->nextPtr) {
    NsfClass *cl = NULL;
    const char *simpleName = Tcl_GetCommandName(interp, cmdList->cmdPtr);
    Tcl_Command cmd = FilterSearch(simpleName, startingObject, startingClass, &cl);

    if (cmd == NULL) {
      del = CmdListRemoveFromList(filters, cmdList);
      CmdListDeleteCmdListEntry(del, GuardDel);
    } else if (cmd != cmdList->cmdPtr) {
      CmdListReplaceCmd(cmdList, cmd, cl);
    }
  }

  /*
   * some entries might be NULL now, if they are not found anymore
   *  -> delete those
   *  CmdListRemoveNulledEntries(filters, GuardDel);
   */
}


/*
 *----------------------------------------------------------------------
 * FilterInvalidateObjOrders --
 *
 *    Invalidate filter entries in all dependent instances. This will be
 *    e.g. necessary, when the class hierarchy or the class filters have
 *    changed.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static void FilterInvalidateObjOrders(Tcl_Interp *interp, NsfClasses *subClasses)
  nonnull(1) nonnull(2);

static void
FilterInvalidateObjOrders(Tcl_Interp *interp, NsfClasses *subClasses) {

  nonnull_assert(interp != NULL);
  nonnull_assert(subClasses != NULL);

  do {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry *hPtr;

    assert(subClasses->cl);

    hPtr = Tcl_FirstHashEntry(&subClasses->cl->instances, &hSrch);

    /* recalculate the commands of all class-filter registrations */
    if (subClasses->cl->opt != NULL) {
      FilterSearchAgain(interp, &subClasses->cl->opt->classFilters, NULL, subClasses->cl);
    }

    for (; hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      NsfObject *object = (NsfObject *)Tcl_GetHashKey(&subClasses->cl->instances, hPtr);

      FilterResetOrder(object);
      object->flags &= ~NSF_FILTER_ORDER_VALID;

      /* recalculate the commands of all object filter registrations */
      if (object->opt != NULL) {
        FilterSearchAgain(interp, &object->opt->objFilters, object, NULL);
      }
    }
    subClasses = subClasses->nextPtr;
  } while (likely(subClasses != NULL));
}

/*
 *----------------------------------------------------------------------
 * FilterRemoveDependentFilterCmds --
 *
 *
 *    Remove all filters from all subclasses that refer to "removeClass". This
 *    function is e.g. used to remove filters defined in superclass list from
 *    a dependent class.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
/*
 */
static void FilterRemoveDependentFilterCmds(NsfClass *removeClass, NsfClasses *subClasses)
  nonnull(1) nonnull(2);

static void
FilterRemoveDependentFilterCmds(NsfClass *removeClass, NsfClasses *subClasses) {

  nonnull_assert(removeClass != NULL);
  nonnull_assert(subClasses != NULL);

  /*fprintf(stderr, "FilterRemoveDependentFilterCmds removeClass %p %s\n",
    removeClass, ObjStr(removeClass->object.cmdName));*/

  for (; likely(subClasses != NULL); subClasses = subClasses->nextPtr) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry *hPtr;
    NsfClassOpt *opt;

    assert(subClasses->cl);
    hPtr = Tcl_FirstHashEntry(&subClasses->cl->instances, &hSrch);

    opt = subClasses->cl->opt;
    if (opt != NULL) {
      CmdListRemoveContextClassFromList(&opt->classFilters, removeClass, GuardDel);
    }
    for (; hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      NsfObject *object = (NsfObject *) Tcl_GetHashKey(&subClasses->cl->instances, hPtr);
      if (object->opt != NULL) {
        CmdListRemoveContextClassFromList(&object->opt->objFilters, removeClass, GuardDel);
      }
    }
  }
}

/*
 *----------------------------------------------------------------------
 * MethodHandleObj --
 *
 *    Builds a methodHandle from a method name. We assume, the methodName is
 *    not fully qualified (i.e. it must not start with a colon).
 *
 * Results:
 *    fresh Tcl_Obj
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj * MethodHandleObj(NsfObject *object, int withPer_object, const char *methodName)
  nonnull(1) nonnull(3) returns_nonnull;

static Tcl_Obj *
MethodHandleObj(NsfObject *object, int withPer_object, const char *methodName) {
  Tcl_Obj *resultObj;

  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  assert(*methodName != ':');

  resultObj = Tcl_NewStringObj((withPer_object == 1) ? "" : "::nsf::classes", -1);
  Tcl_AppendObjToObj(resultObj, object->cmdName);
  Tcl_AppendStringsToObj(resultObj, "::", methodName, (char *) NULL);

  return resultObj;
}

/*
 *----------------------------------------------------------------------
 * FilterInfo --
 *
 *    Set the interp results with a tcl list containing the content of the
 *    filter list. The options withGuards and withMethodHandles can be used
 *    for different output structures
 *
 * Results:
 *    Standard Tcl results
 *
 * Side effects:
 *    Updating interp result
 *
 *----------------------------------------------------------------------
 */
static int FilterInfo(Tcl_Interp *interp, NsfCmdList *f, const char *pattern,
           int withGuards, int withMethodHandles) nonnull(1);

static int
FilterInfo(Tcl_Interp *interp, NsfCmdList *f, const char *pattern,
           int withGuards, int withMethodHandles) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);

  /*
   * Guard lists should only have unqualified filter lists when withGuards is
   * activated. withMethodHandles has no effect when withGuards is specified.
   */
  if (withGuards != 0) {
    withMethodHandles = 0;
  }

  while (f) {
    const char *simpleName = Tcl_GetCommandName(interp, f->cmdPtr);

    if (pattern == NULL || Tcl_StringMatch(simpleName, pattern)) {
      if (withGuards && f->clientData) {
        Tcl_Obj *innerList = Tcl_NewListObj(0, NULL);
        Tcl_Obj *g = (Tcl_Obj *) f->clientData;

        Tcl_ListObjAppendElement(interp, innerList,
                                 Tcl_NewStringObj(simpleName, -1));
        Tcl_ListObjAppendElement(interp, innerList, NsfGlobalObjs[NSF_GUARD_OPTION]);
        Tcl_ListObjAppendElement(interp, innerList, g);
        Tcl_ListObjAppendElement(interp, list, innerList);
      } else {
        if (withMethodHandles != 0) {
          NsfClass *filterClass = f->clorobj;

          Tcl_ListObjAppendElement(interp, list,
                                   MethodHandleObj((NsfObject *)filterClass,
                                                   !NsfObjectIsClass(&filterClass->object), simpleName));
        } else {
          Tcl_ListObjAppendElement(interp, list, Tcl_NewStringObj(simpleName, -1));
        }
      }
    }
    f = f->nextPtr;
  }
  Tcl_SetObjResult(interp, list);
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * FilterComputeOrderFullList --
 *
 *    Compute a fresh list of filters and append it to the filterList.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Updating filterList
 *
 *----------------------------------------------------------------------
 */
static void FilterComputeOrderFullList(Tcl_Interp *interp, NsfCmdList **filters,
                                       NsfCmdList **filterList)
  nonnull(1) nonnull(2) nonnull(3);

static void
FilterComputeOrderFullList(Tcl_Interp *interp, NsfCmdList **filters,
                           NsfCmdList **filterList) {
  NsfCmdList *f ;
  NsfClass *fcl;
  NsfClasses *pl;

  nonnull_assert(interp != NULL);
  nonnull_assert(filters != NULL);
  nonnull_assert(filterList != NULL);

  /*
   * Ensure that no epoched command is in the filters list.
   */
  CmdListRemoveDeleted(filters, GuardDel);

  for (f = *filters; f; f = f->nextPtr) {
    const char *simpleName = Tcl_GetCommandName(interp, f->cmdPtr);
    fcl = f->clorobj;
    CmdListAdd(filterList, f->cmdPtr, fcl, /*noDuplicates*/ 0, 1);

    if (fcl && !NsfObjectIsClass(&fcl->object)) {
      /* get the class from the object for per-object filter */
      fcl = ((NsfObject *)fcl)->cl;
    }

    /* if we have a filter class -> search up the inheritance hierarchy*/
    if (fcl != NULL) {
      pl = PrecedenceOrder(fcl);
      if (pl && pl->nextPtr) {
        /* don't search on the start class again */
        pl = pl->nextPtr;
        /* now go up the hierarchy */
        for(; pl; pl = pl->nextPtr) {
          Tcl_Command pi = FindMethod(pl->cl->nsPtr, simpleName);
          if (pi != NULL) {
            CmdListAdd(filterList, pi, pl->cl, /*noDuplicates*/ 0, 1);
            /*
              fprintf(stderr, " %s::%s, ", ClassName(pl->cl), simpleName);
            */
          }
        }
      }
    }
  }
  /*CmdListPrint(interp, "FilterComputeOrderFullList....\n", *filterList);*/
}

/*
 *----------------------------------------------------------------------
 * FilterComputeOrder --
 *
 *    Computes a linearized order of object and class filter. Then duplicates
 *    in the full list and with the class inheritance list of 'obj' are
 *    eliminated.  The precedence rule is that the last occurrence makes it
 *    into the final list (object->filterOrder).
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Updating interp result
 *
 *----------------------------------------------------------------------
 */
static void FilterComputeOrder(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static void
FilterComputeOrder(Tcl_Interp *interp, NsfObject *object) {
  NsfCmdList *filterList = NULL, *next, *checker, *newList;
  NsfClasses *pl;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->filterOrder != NULL) {
    FilterResetOrder(object);
  }
  /*
    fprintf(stderr, "<Filter Order obj=%s> List: ", ObjectName(object));
  */

  /*
   * Append class filters registered for mixins.
   */
  if ((object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
    MixinComputeDefined(interp, object);
  }
  if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
    NsfCmdList *ml;

    for (ml = object->mixinOrder; ml != NULL; ml = ml->nextPtr) {
      NsfClass *mixin = NsfGetClassFromCmdPtr(ml->cmdPtr);

      if (mixin != NULL && mixin->opt != NULL && mixin->opt->classFilters != NULL) {
        FilterComputeOrderFullList(interp, &mixin->opt->classFilters, &filterList);
      }
    }
  }

  /*
   * Append per-obj filters.
   */
  if (object->opt != NULL) {
    FilterComputeOrderFullList(interp, &object->opt->objFilters, &filterList);
  }

  /*
   * Append per-class filters.
   */
  for (pl = PrecedenceOrder(object->cl); pl; pl = pl->nextPtr) {
    NsfClassOpt *clopt = pl->cl->opt;
    if (clopt != NULL && clopt->classFilters != NULL) {
      FilterComputeOrderFullList(interp, &clopt->classFilters, &filterList);
    }
  }

  /*
   * Use no duplicates & no classes of the precedence order
   * on the resulting list.
   */
  while (filterList) {
    /*
     * Search for filterList->cmdPtr
     */
    for (checker = next = filterList->nextPtr; checker; checker = checker->nextPtr) {
      if (checker->cmdPtr == filterList->cmdPtr) {
        break;
      }
    }

    if (checker == NULL) {
      /*
       * filterList->cmdPtr was found
       */
      newList = CmdListAdd(&object->filterOrder, filterList->cmdPtr, filterList->clorobj,
                           /*noDuplicates*/ 0, 1);
      GuardAddInheritedGuards(interp, newList, object, filterList->cmdPtr);
      /*
        GuardPrint(interp, newList->clientData);
      */
    }

    CmdListDeleteCmdListEntry(filterList, GuardDel);
    filterList = next;
  }
}

/*
 *----------------------------------------------------------------------
 * FilterComputeDefined --
 *
 *    Compute the state of the filter order. The filter order is either
 *
 *       DEFINED (there are filter on the instance),
 *       NONE    (there are no filter for the instance),
 *       or INVALID (a class restructuring has occurred, thus it is not clear
 *                whether filters are defined or not).
 *
 *    If it is INVALID FilterComputeDefined can be used to compute the order
 *    and set the instance to DEFINE or NONE.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Updating object-flags
 *
 *----------------------------------------------------------------------
 */
static void
FilterComputeDefined(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  FilterComputeOrder(interp, object);
  object->flags |= NSF_FILTER_ORDER_VALID;
  if (object->filterOrder != NULL) {
    object->flags |= NSF_FILTER_ORDER_DEFINED;
  } else {
    object->flags &= ~NSF_FILTER_ORDER_DEFINED;
  }
}

/*
 *----------------------------------------------------------------------
 * FilterStackPush --
 *
 *    Push a filter stack information on this object and initialize it with
 *    calledProc.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Updating object->filterStack
 *
 *----------------------------------------------------------------------
 */
static void FilterStackPush(NsfObject *object, Tcl_Obj *calledProc) nonnull(1) nonnull(2);

static void
FilterStackPush(NsfObject *object, Tcl_Obj *calledProc) {
  register NsfFilterStack *h = NEW(NsfFilterStack);

  nonnull_assert(object != NULL);
  nonnull_assert(calledProc != NULL);

  h->currentCmdPtr = NULL;
  h->calledProc = calledProc;
  INCR_REF_COUNT(h->calledProc);
  h->nextPtr = object->filterStack;
  object->filterStack = h;
}

/*
 *----------------------------------------------------------------------
 * FilterStackPush --
 *
 *    Pop filter stack information from the specified object
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Free filter stack info
 *
 *----------------------------------------------------------------------
 */
static void FilterStackPop(NsfObject *object) nonnull(1);

static void
FilterStackPop(NsfObject *object) {
  register NsfFilterStack *h;

  nonnull_assert(object != NULL);

  h = object->filterStack;
  object->filterStack = h->nextPtr;

  /* free stack entry */
  DECR_REF_COUNT(h->calledProc);
  FREE(NsfFilterStack, h);
}

/*
 *----------------------------------------------------------------------
 * FilterFindReg --
 *
 *    Search through the filter list on obj and class hierarchy for
 *    registration of a cmdPtr as filter
 *
 * Results:
 *    Returns a tcl list with the filter registration, like:
 *    "<obj> filter <filterName>, "<class> filter <filterName>,
 *    or an empty list, if not registered
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj * FilterFindReg(Tcl_Interp *interp, NsfObject *object, Tcl_Command cmd)
  nonnull(1) nonnull(2) nonnull(3) returns_nonnull;

static Tcl_Obj *
FilterFindReg(Tcl_Interp *interp, NsfObject *object, Tcl_Command cmd) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);
  NsfClasses *pl;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(cmd != NULL);

  /* search per-object filters */
  if (object->opt != NULL
      && object->opt->objFilters != NULL
      && CmdListFindCmdInList(cmd, object->opt->objFilters)) {
    Tcl_ListObjAppendElement(interp, list, object->cmdName);
    Tcl_ListObjAppendElement(interp, list, NsfGlobalObjs[NSF_OBJECT]);
    Tcl_ListObjAppendElement(interp, list, NsfGlobalObjs[NSF_FILTER]);
    Tcl_ListObjAppendElement(interp, list,
                             Tcl_NewStringObj(Tcl_GetCommandName(interp, cmd), -1));
    return list;
  }

  /* search per-class filters */
  for (pl = PrecedenceOrder(object->cl); pl; pl = pl->nextPtr) {
    NsfClassOpt *opt = pl->cl->opt;
    if (opt != NULL && opt->classFilters != NULL) {
      if (CmdListFindCmdInList(cmd, opt->classFilters)) {
        Tcl_ListObjAppendElement(interp, list, pl->cl->object.cmdName);
        Tcl_ListObjAppendElement(interp, list, NsfGlobalObjs[NSF_FILTER]);
        Tcl_ListObjAppendElement(interp, list,
                                 Tcl_NewStringObj(Tcl_GetCommandName(interp, cmd), -1));
        return list;
      }
    }
  }
  return list;
}

/*
 *----------------------------------------------------------------------
 * FilterSearchProc --
 *
 *     FilterSearchProc seeks the current filter and the relevant calling
 *     information (class and currentCmd). The function assumes to be called
 *     with an existing filterStack.
 *
 * Results:
 *    Tcl_Command or NULL
 *
 * Side effects:
 *    Updates *currentCmd and **cl
 *
 *----------------------------------------------------------------------
 */
/*
 */
static Tcl_Command FilterSearchProc(Tcl_Interp *interp, NsfObject *object,
                 Tcl_Command *currentCmd, NsfClass **clPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static Tcl_Command
FilterSearchProc(Tcl_Interp *interp, NsfObject *object,
                 Tcl_Command *currentCmd, NsfClass **clPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(currentCmd != NULL);
  nonnull_assert(clPtr != NULL);
  
  assert(object->filterStack != NULL);
  /*
   * Ensure that the filter order is not invalid, otherwise compute order
   * FilterComputeDefined(interp, object);
   */
  assert(object->flags & NSF_FILTER_ORDER_VALID);

  if (object->filterOrder != NULL) {
    NsfCmdList *cmdList;

    *currentCmd = NULL;
    cmdList = SeekCurrent(object->filterStack->currentCmdPtr, object->filterOrder);

    while (cmdList) {
      /*fprintf(stderr, "FilterSearchProc found %s\n",
        Tcl_GetCommandName(interp, (Tcl_Command)cmdList->cmdPtr));*/
      if (Tcl_Command_cmdEpoch(cmdList->cmdPtr)) {
        cmdList = cmdList->nextPtr;
      } else if (FilterActiveOnObj(interp, object, cmdList->cmdPtr)) {
        /* fprintf(stderr, "Filter <%s> -- Active on: %s\n",
           Tcl_GetCommandName(interp, (Tcl_Command)cmdList->cmdPtr), ObjectName(object));
        */
        object->filterStack->currentCmdPtr = cmdList->cmdPtr;
        cmdList = SeekCurrent(object->filterStack->currentCmdPtr, object->filterOrder);
      } else {
        /* ok. we found it */
        if (cmdList->clorobj && !NsfObjectIsClass(&cmdList->clorobj->object)) {
          *clPtr = NULL;
        } else {
          *clPtr = cmdList->clorobj;
        }
        *currentCmd = cmdList->cmdPtr;
        /* fprintf(stderr, "FilterSearchProc - found: %s, %p\n",
           Tcl_GetCommandName(interp, (Tcl_Command)cmdList->cmdPtr), cmdList->cmdPtr);
        */
        return cmdList->cmdPtr;
      }
    }
  }
  return NULL;
}

/*
 *----------------------------------------------------------------------
 * SuperclassAdd --
 *
 *    Add a list of superClasses (specified in the argument vector) to
 *    the specified class. On the first call, the class has no previous
 *    superClasses.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Rearranging the class relations, flushing previous precedence orders.
 *
 *----------------------------------------------------------------------
 */
static int SuperclassAdd(Tcl_Interp *interp, NsfClass *cl, int oc, Tcl_Obj **ov, Tcl_Obj *arg)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);

static int
SuperclassAdd(Tcl_Interp *interp, NsfClass *cl, int oc, Tcl_Obj **ov, Tcl_Obj *arg) {
  NsfClasses *superClasses, *subClasses, *osl = NULL;
  NsfObjectSystem *osPtr;
  NsfClass **scl;
  int i, j;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(ov != NULL);
  nonnull_assert(arg != NULL);

  superClasses = PrecedenceOrder(cl);
  subClasses = DependentSubClasses(cl);

  /*
   * We have to remove all dependent superclass filter referenced
   * by class or one of its subclasses.
   *
   * Do not check the class "cl" itself (first entry in
   * filterCheck class list).
   */
  if (superClasses != NULL) {
    superClasses = superClasses->nextPtr;
  }
  for (; superClasses; superClasses = superClasses->nextPtr) {
    FilterRemoveDependentFilterCmds(superClasses->cl, subClasses);
  }

  /*
   * Invalidate all interceptors orders of instances of this and of all
   * depended classes.
   */
  MixinInvalidateObjOrders(interp, cl, subClasses);
  if (FiltersDefined(interp) > 0) {
    FilterInvalidateObjOrders(interp, subClasses);
  }

  /*
   * Build an array of superClasses from the argument vector.
   */
  scl = NEW_ARRAY(NsfClass*, oc);
  for (i = 0; i < oc; i++) {
    if (GetClassFromObj(interp, ov[i], &scl[i], 1) != TCL_OK) {
      FREE(NsfClass**, scl);
      NsfClassListFree(subClasses);
      return NsfObjErrType(interp, "superclass", arg, "a list of classes", NULL);
    }
  }

  /*
   * Check that superClasses don't precede their classes.
   */
  for (i = 0; i < oc; i++) {
    for (j = i+1; j < oc; j++) {
      NsfClasses *dl = PrecedenceOrder(scl[j]);
      dl = NsfClassListFind(dl, scl[i]);
      if (dl != NULL) {
        FREE(NsfClass**, scl);
        NsfClassListFree(subClasses);
        return NsfObjErrType(interp, "superclass", arg, "classes in dependence order", NULL);
      }
    }
  }

  /*
   * Ensure that the current class and new superClasses are from the
   * same object system.
   */
  osPtr = GetObjectSystem(&cl->object);
  for (i = 0; i < oc; i++) {
    if (osPtr != GetObjectSystem(&scl[i]->object)) {
      NsfPrintError(interp, "class \"%s\" has a different object system as class  \"%s\"",
                           ClassName(cl), ClassName(scl[i]));
      NsfClassListFree(subClasses);
      FREE(NsfClass**, scl);
      return TCL_ERROR;
    }
  }

  while (cl->super) {
    /*
     * Build a backup of the old superclass list in case we need to revert.
     */
    NsfClass *sc = cl->super->cl;
    NsfClasses *l = osl;

    osl = NEW(NsfClasses);
    osl->cl = sc;
    osl->nextPtr = l;
    (void)RemoveSuper(cl, cl->super->cl);
  }

  for (i = 0; i < oc; i++) {
    AddSuper(cl, scl[i]);
  }

  FlushPrecedences(subClasses);
  NsfClassListFree(subClasses);
  FREE(NsfClass**, scl);

  if (unlikely(!PrecedenceOrder(cl))) {
    NsfClasses *l;
    /*
     * There is a cycle in the superclass graph, we have to revert and return
     * an error.
     */
    while (cl->super) {
      (void)RemoveSuper(cl, cl->super->cl);
    }
    for (l = osl; l; l = l->nextPtr) {
      AddSuper(cl, l->cl);
    }
    if (osl != NULL) {
      NsfClassListFree(osl);
    }

    return NsfObjErrType(interp, "superclass", arg, "a cycle-free graph", NULL);
  }

  if (osl != NULL) {
    NsfClassListFree(osl);
  }

  assert(cl->super);

  Tcl_ResetResult(interp);
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * CheckVarName --
 *
 *    Check, whether the provided name is free of namespace markup.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int CheckVarName(Tcl_Interp *interp, const char *varNameString) nonnull(1) nonnull(2);

static int
CheckVarName(Tcl_Interp *interp, const char *varNameString) {

  nonnull_assert(interp != NULL);
  nonnull_assert(varNameString != NULL);

  /*
   * We want to have a plain variable name, since we do not want to
   * get interferences with namespace resolver and such.  In an first
   * attempt, we disallowed occurrences of "::", but we have to deal as
   * well with e.g. arrayName(::x::y)
   *
   * TODO: more general and efficient solution to disallow e.g. a::b
   * (check for :: until parens)
   */
  /*if (strstr(varNameString, "::") || *varNameString == ':') {*/
  if (*varNameString == ':') {
    return NsfPrintError(interp, "variable name \"%s\" must not contain "
                         "namespace separator or colon prefix",
                         varNameString);
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * VarExists --
 *
 *    Check, whether the named variable exists on the specified object.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int VarExists(Tcl_Interp *interp, NsfObject *object, const char *name1, const char *name2, unsigned int flags)
  nonnull(1) nonnull(2) nonnull(3);

static int
VarExists(Tcl_Interp *interp, NsfObject *object, const char *name1, const char *name2,
          unsigned int flags) {
  CallFrame frame, *framePtr = &frame;
  Var *varPtr, *arrayPtr;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(name1 != NULL);

  Nsf_PushFrameObj(interp, object, framePtr);

  if ((flags & NSF_VAR_TRIGGER_TRACE) != 0u) {
    varPtr = TclVarTraceExists(interp, name1);
  } else {
    unsigned int flags = (name2 == NULL) ? TCL_PARSE_PART1 : 0;
    varPtr = TclLookupVar(interp, name1, name2, flags, "access",
                          /*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
  }
  /*
    fprintf(stderr, "VarExists %s varPtr %p flags %.4x isundef %d\n",
    name1,
    varPtr,
    flags, (varPtr != NULL) ? TclIsVarUndefined(varPtr) : NULL);
  */
  result = ((varPtr != NULL) && ((flags & NSF_VAR_REQUIRE_DEFINED) == 0u || !TclIsVarUndefined(varPtr)));
  if (result && ((flags & NSF_VAR_ISARRAY) != 0u) && !TclIsVarArray(varPtr)) {
    result = 0;
  }
  Nsf_PopFrameObj(interp, framePtr);

  return result;
}

#if defined(WITH_TCL_COMPILE)
# include <tclCompile.h>
#endif

/*
 *----------------------------------------------------------------------
 * MakeProcError --
 *
 *    Function called internally from Tcl in case the definition of the proc
 *    failed.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static void
MakeProcError(
              Tcl_Interp *interp,        /* The interpreter in which the procedure was called. */
              Tcl_Obj *procNameObj)        /* Name of the procedure. Used for error
                                         * messages and trace information. */
{
  int overflow, limit = 60, nameLen;
  const char *procName;

  /*fprintf(stderr, "MakeProcError %p type %p refCount %d\n",
    procNameObj, procNameObj->typePtr, procNameObj->refCount);*/

  procName = Tcl_GetString(procNameObj);
  nameLen = procNameObj->length;
  overflow = (nameLen > limit);
  Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
                                                 "\n    (procedure \"%.*s%s\" line %d)",
                                                 ((overflow != 0) ? limit : nameLen), procName,
                                                 ((overflow != 0) ? "..." : ""), Tcl_GetErrorLine(interp)));
}

/*
 *----------------------------------------------------------------------
 * ByteCompiled --
 *
 *    Function to determine wether a proc is already byted compiled or not.
 *
 * Results:
 *    0 or 1 based on success
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int ByteCompiled(Tcl_Interp *interp, unsigned int *flagsPtr,
                        Proc *procPtr, const char *procName)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
ByteCompiled(Tcl_Interp *interp, unsigned int *flagsPtr,
             Proc *procPtr, const char *procName) {
  Namespace *nsPtr;
  Tcl_Obj *bodyObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(flagsPtr != NULL);
  nonnull_assert(procPtr != NULL);
  nonnull_assert(procName != NULL);

  nsPtr = procPtr->cmdPtr->nsPtr;
  bodyObj = procPtr->bodyPtr;

  if (likely(bodyObj->typePtr == Nsf_OT_byteCodeType)) {
#if defined(HAVE_TCL_COMPILE_H)
    ByteCode *codePtr;
    Interp *iPtr = (Interp *) interp;

    /*
     * When we've got bytecode, this is the check for validity. That is,
     * the bytecode must be for the right interpreter (no cross-leaks!),
     * the code must be from the current epoch (so subcommand compilation
     * is up-to-date), the namespace must match (so variable handling
     * is right) and the resolverEpoch must match (so that new shadowed
     * commands and/or resolver changes are considered).
     */

    codePtr = bodyObj->internalRep.otherValuePtr;
    if (unlikely(((Interp *) *codePtr->interpHandle != iPtr)
                 || (codePtr->compileEpoch != iPtr->compileEpoch)
                 || (codePtr->nsPtr != nsPtr)
                 || (codePtr->nsEpoch != nsPtr->resolverEpoch))) {

# if defined(VAR_RESOLVER_TRACE)
      fprintf(stderr, "ByteCompiled bytecode not valid proc %p cmd %p method %s\n",
              procPtr, procPtr->cmdPtr,
              Tcl_GetCommandName(interp, (Tcl_Command)procPtr->cmdPtr));
      fprintf(stderr, "    %d %d %d %d\n",
              ((Interp *) *codePtr->interpHandle != iPtr),
               (codePtr->compileEpoch != iPtr->compileEpoch),
               (codePtr->nsPtr != nsPtr),
               (codePtr->nsEpoch != nsPtr->resolverEpoch));

      {
        CompiledLocal *localPtr = procPtr->firstLocalPtr;
        for (; localPtr != NULL; localPtr = localPtr->nextPtr) {
          fprintf(stderr, "... local %p '%s' resolveInfo %p deleteProc %p\n",
                  localPtr, localPtr->name, localPtr->resolveInfo,
                  (localPtr->resolveInfo != NULL) ? localPtr->resolveInfo->deleteProc : NULL);
        }
      }
# endif
      /* dummy statement for coverage analysis */
      assert(1);
      goto doCompilation;
    }
#endif
    return TCL_OK;
  } else {
    int result;
#if defined(HAVE_TCL_COMPILE_H)
  doCompilation:
#endif

    *flagsPtr |= NSF_CSC_CALL_IS_COMPILE;
    result = TclProcCompileProc(interp, procPtr, bodyObj,
                              (Namespace *) nsPtr, "body of proc",
                              procName);
    *flagsPtr &= ~NSF_CSC_CALL_IS_COMPILE;

    return result;
  }
}

/*
 *----------------------------------------------------------------------
 * PushProcCallFrame --
 *
 *    Set up and push a new call frame for the procedure invocation.
 *    call-frame. The proc is passed via clientData.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    compiles body conditionally
 *
 *----------------------------------------------------------------------
 */
static int PushProcCallFrame(Proc *procPtr, Tcl_Interp *interp,
                  int objc, Tcl_Obj *CONST objv[],
                  NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);

static int
PushProcCallFrame(Proc *procPtr, Tcl_Interp *interp,
                  int objc, Tcl_Obj *CONST objv[],
                  NsfCallStackContent *cscPtr) {
  Tcl_CallFrame *framePtr;
  int result;

  nonnull_assert(procPtr != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(cscPtr != NULL);

  /*
   * Set up and push a new call frame for the new procedure invocation.
   * This call frame will execute in the proc's namespace, which might be
   * different than the current namespace. The proc's namespace is that of
   * its command, which can change if the command is renamed from one
   * namespace to another.
   */

  /* TODO: we could use Tcl_PushCallFrame(), if we would allocate the tcl stack frame earlier */
  result = TclPushStackFrame(interp, (Tcl_CallFrame **)&framePtr,
                             (Tcl_Namespace *) procPtr->cmdPtr->nsPtr,
                             (FRAME_IS_PROC|FRAME_IS_NSF_METHOD));

  if (unlikely(result != TCL_OK)) {
    return result;
  }

  Tcl_CallFrame_objc(framePtr) = objc;
  Tcl_CallFrame_objv(framePtr) = objv;
  Tcl_CallFrame_procPtr(framePtr) = procPtr;
  Tcl_CallFrame_clientData(framePtr) = cscPtr;

  /*fprintf(stderr, "Stack Frame %p procPtr %p compiledLocals %p firstLocal %p\n",
    framePtr, procPtr, Tcl_CallFrame_compiledLocals(framePtr), procPtr->firstLocalPtr);*/

  return ByteCompiled(interp, &cscPtr->flags, procPtr, ObjStr(objv[0]));
}

#include "nsfAPI.h"

/*
 *----------------------------------------------------------------------
 * ObjectSystemsCheckSystemMethod --
 *
 *    Mark the specified method as (potentially) 'overloaded' in all object
 *    systems and declare it 'defined' in the specified object system.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Updates the object system structure(s).
 *
 *----------------------------------------------------------------------
 */

static int
ObjectSystemsCheckSystemMethod(Tcl_Interp *interp, const char *methodName, NsfObject *object, unsigned int flags) {
  NsfObjectSystem *osPtr, *defOsPtr;
  char firstChar;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);

  firstChar = *methodName;
  defOsPtr = GetObjectSystem(object);

  for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = osPtr->nextPtr) {
    int i, isRootClassMethod;
    unsigned int flag = 0u;
    NsfObject *defObject;

    for (i = 0; i <= NSF_s_set_idx; i++) {
      Tcl_Obj *methodObj = osPtr->methods[i];
      const char *methodString = (methodObj != NULL) ? ObjStr(methodObj) : NULL;

      if (methodString && *methodString == firstChar && !strcmp(methodName, methodString)) {
        flag = 1<<i;
        break;
      }
    }
    if (flag == 0u) {
      continue;
    }

    isRootClassMethod = *(Nsf_SystemMethodOpts[i]+1) == 'o';
    defObject = (isRootClassMethod == 1) ? &osPtr->rootClass->object
      : &osPtr->rootMetaClass->object;

    if (osPtr->handles[i] && osPtr->protected[i]) {
      if (defObject == object && (flags & NSF_CMD_REDEFINE_PROTECTED_METHOD) == 0u) {
        return NsfPrintError(interp, "refuse to overwrite protected method %s on %s",
                             methodName, ObjectName(defObject));
      }
    }

    if ((osPtr->definedMethods & flag) != 0u) {
      /*
       *  If for some reason base methods become redefined (e.g. in a reload),
       *  do not count them as overloads.
       */
      if ((isRootClassMethod == 1 && object == &defOsPtr->rootClass->object)
          || (isRootClassMethod == 0 && object == &defOsPtr->rootMetaClass->object) ) {
        /*fprintf(stderr, "+++ %s %.6x NOT overloading %s.%s %s (is root %d, is meta %d)\n",
          ClassName(defOsPtr->rootClass),
          osPtr->overloadedMethods, ObjectName(object), methodName, Nsf_SystemMethodOpts[i],
          object == &defOsPtr->rootClass->object,
          object == &defOsPtr->rootMetaClass->object);*/
      } else {
        osPtr->overloadedMethods |= flag;
        /*fprintf(stderr, "+++ %s %.6x overloading %s.%s %s (is root %d, is meta %d)\n",
          ClassName(defOsPtr->rootClass),
          osPtr->overloadedMethods, ObjectName(object), methodName, Nsf_SystemMethodOpts[i],
          object == &defOsPtr->rootClass->object,
          object == &defOsPtr->rootMetaClass->object);*/
      }
    }
    if ((osPtr == defOsPtr) && ((osPtr->definedMethods & flag) == 0u)) {
      /*
       * Mark the method as defined.
       */
      osPtr->definedMethods |= flag;

      /*fprintf(stderr, "+++ %s %.6x defining %s.%s %s osPtr %p defined %.8x flag %.8x handle %p\n",
              ClassName(defOsPtr->rootClass),  osPtr->definedMethods, ObjectName(object),
              methodName, Nsf_SystemMethodOpts[i], osPtr, osPtr->definedMethods, flag,
              osPtr->handles[i]);*/

      /*
       * If there is a method handle provided for this system method, register
       * it as a fallback; unless the method is to be defined at the root
       * class.
       */
      if (osPtr->handles[i]) {

        if (defObject != object) {
          int result;

          NsfLog(interp, NSF_LOG_NOTICE, "Define automatically alias %s for %s",
                 ObjStr(osPtr->handles[i]), Nsf_SystemMethodOpts[i]);

          result = NsfMethodAliasCmd(interp, defObject, 0, methodName, 0,
                                         ProtectionRedefine_protectedIdx, osPtr->handles[i]);

          if (unlikely(result != TCL_OK)) {
            /* alias definition failed */
            NsfLog(interp, NSF_LOG_WARN, "Could not define alias %s for %s",
                   ObjStr(osPtr->handles[i]), Nsf_SystemMethodOpts[i]);
            return TCL_ERROR;
          } else {
            /*
             * Alias definition suceeded
             */
            Tcl_Obj *methodObj = Tcl_GetObjResult(interp);
            Tcl_Command cmd = Tcl_GetCommandFromObj(interp, methodObj);


            /*
             * Since the defObject is not equal to the overloaded method, the
             * definition above is effectively an overload of the alias.
             */
            osPtr->overloadedMethods |= flag;

            /*
             * Set method protection.
             */
            if (cmd != NULL) {
              Tcl_Command_flags(cmd) |= NSF_CMD_CALL_PROTECTED_METHOD;
              if (osPtr->protected[i]) {
                Tcl_Command_flags(cmd) |= NSF_CMD_REDEFINE_PROTECTED_METHOD;
              }
            }
            Tcl_ResetResult(interp);
          }
        }
      }
    }
  }

  return TCL_OK;
}


/*----------------------------------------------------------------------
 * ParamsNew --
 *
 *    Allocate an array of Nsf_Param structures
 *
 * Results:
 *    Pointer to allocated memory
 *
 * Side effects:
 *    Allocation of memory.
 *
 *----------------------------------------------------------------------
 */

static Nsf_Param *
ParamsNew(int nr) {
  Nsf_Param *paramsPtr = NEW_ARRAY(Nsf_Param, nr+1);

  memset(paramsPtr, 0, sizeof(Nsf_Param)*(nr+1));

  return paramsPtr;
}

/*----------------------------------------------------------------------
 * ParamFree --
 *
 *    Deallocate the contents of a single Nsf_Param*
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free the parameter definition.
 *
 *----------------------------------------------------------------------
 */
static void ParamFree(Nsf_Param *paramPtr) nonnull(1);

static void
ParamFree(Nsf_Param *paramPtr) {

  nonnull_assert(paramPtr != NULL);

  /*fprintf(stderr, "ParamFree %p\n", paramPtr);*/
  if (paramPtr->name != NULL) {STRING_FREE("paramPtr->name", paramPtr->name);}
  if (paramPtr->nameObj != NULL) {DECR_REF_COUNT(paramPtr->nameObj);}
  if (paramPtr->defaultValue != NULL) {DECR_REF_COUNT(paramPtr->defaultValue);}
  if (paramPtr->converterName != NULL) {DECR_REF_COUNT2("converterNameObj", paramPtr->converterName);}
  if (paramPtr->converterArg != NULL) {DECR_REF_COUNT(paramPtr->converterArg);}
  if (paramPtr->paramObj != NULL) {DECR_REF_COUNT(paramPtr->paramObj);}
  if (paramPtr->slotObj != NULL) {DECR_REF_COUNT(paramPtr->slotObj);}
  if (paramPtr->method != NULL) {DECR_REF_COUNT(paramPtr->method);}
}

/*----------------------------------------------------------------------
 * ParamsFree --
 *
 *    Deallocate a block of multiple Nsf_Param*
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free the parameter definition.
 *
 *----------------------------------------------------------------------
 */
static void ParamsFree(Nsf_Param *paramsPtr) nonnull(1);

static void
ParamsFree(Nsf_Param *paramsPtr) {
  Nsf_Param *paramPtr;

  nonnull_assert(paramsPtr != NULL);

  /*fprintf(stderr, "ParamsFree %p\n", paramsPtr);*/
  for (paramPtr = paramsPtr; paramPtr->name; paramPtr++) {
    ParamFree(paramPtr);
  }

  FREE(Nsf_Param*, paramsPtr);
}

/*----------------------------------------------------------------------
 * ParamDefsGet --
 *
 *    Obtain parameter definitions for a cmdPtr; Optionally, this command
 *    returns as well a flag for ProcessMethodArguments to indicate if the
 *    parameter have to checked always.
 *
 * Results:
 *    Parameter definitions or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static NsfParamDefs *ParamDefsGet(Tcl_Command cmdPtr, int *checkAlwaysFlagPtr) nonnull(1);

NSF_INLINE static NsfParamDefs *
ParamDefsGet(Tcl_Command cmdPtr, int *checkAlwaysFlagPtr) {

  nonnull_assert(cmdPtr != NULL);

  if (likely(Tcl_Command_deleteProc(cmdPtr) == NsfProcDeleteProc)) {
    NsfProcContext *ctx = (NsfProcContext *)Tcl_Command_deleteData(cmdPtr);

    if (checkAlwaysFlagPtr != NULL) { *checkAlwaysFlagPtr = ctx->checkAlwaysFlag;}
    return ctx->paramDefs;
  }

  return NULL;
}

/*----------------------------------------------------------------------
 * NsfParamDefsFilter --
 *
 *    Process a list of ParamDefs and return a subset of it matching the
 *    provided pattern.  If no parameter name matches the pattern, NULL is
 *    returned. The client is supposed to FREE the returned parameter list
 *    (entries are shared, a free of the returned pointer is sufficient).
 *
 * Results:
 *    Parameter definitions or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Nsf_Param *NsfParamDefsFilter(Tcl_Interp *interp, Nsf_Param const *paramsPtr, const char *pattern)
  nonnull(1) nonnull(2) nonnull(3);

static Nsf_Param *
NsfParamDefsFilter(Tcl_Interp *interp, Nsf_Param const *paramsPtr, const char *pattern) {
  static Nsf_Param *paramList = NULL;
  Nsf_Param const *pPtr;
  int maxParams, nrMatchingParams;

  /*
   * If a single parameter or pattern name is given, we construct a filtered
   * parameter list on the fly and we return it
   */

  /*
   * Count the parameters
   */
  for (pPtr = paramsPtr, maxParams = 0; pPtr->name; pPtr++, maxParams++);

  /*
   * Allocate the number of potentional matches
   */
  paramList = ParamsNew(maxParams);

  for (pPtr = paramsPtr, nrMatchingParams = 0; pPtr->name; pPtr++) {
    if (Tcl_StringMatch( ObjStr(pPtr->nameObj), pattern)) {
      paramList[nrMatchingParams] = *pPtr;
      nrMatchingParams++;
    }
  }

  if (nrMatchingParams == 0) {
    /*
     * The named parameter were NOT found, so return NULL
     */
    FREE(Nsf_Param*, paramList);
    paramList = NULL;
  }

  return paramList;
}

/*----------------------------------------------------------------------
 * NsfParamDefsNonposLookup --
 *
 *    Process a list of ParamDefs look for a nonpos args. If there is no exact
 *    match, look for an abbreviated match having at least
 *    NSF_ABBREV_MIN_CHARS leading chars which are identical.
 *
 * Results:
 *    Parameter definition or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int NsfParamDefsNonposLookup(Tcl_Interp *interp, const char *nameString,
                                    Nsf_Param const *paramsPtr,  Nsf_Param const **paramPtrPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
NsfParamDefsNonposLookup(Tcl_Interp *interp, const char *nameString,
                         Nsf_Param const *paramsPtr, Nsf_Param const **paramPtrPtr) {
  Nsf_Param const *paramPtr;
  char             ch1;
  int              length;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameString != NULL);
  nonnull_assert(paramsPtr != NULL);
  nonnull_assert(paramPtrPtr != NULL);

  /*
   * The provided paramsPtr must point to a block starting with a nonpos arg.
   */
  assert(paramsPtr->name != NULL);
  assert(*paramsPtr->name == '-');

  /*
   * The provided nameString starts as well with a leading dash.
   */
  assert(*nameString == '-');

  ch1 = nameString[2];
  for (paramPtr = paramsPtr; likely(paramPtr->name != NULL) && *paramPtr->name == '-'; paramPtr++) {
    if (unlikely((paramPtr->flags & NSF_ARG_NOCONFIG) != 0u)) {
      continue;
    }
    if (ch1 == paramPtr->name[2]
        && strcmp(nameString, paramPtr->name) == 0) {
        *paramPtrPtr = paramPtr;
        return TCL_OK;
    }
  }

  length = strlen(nameString);

  if (length >= NSF_ABBREV_MIN_CHARS) {

    for (paramPtr = paramsPtr; likely(paramPtr->name != NULL) && *paramPtr->name == '-'; paramPtr++) {
      if (unlikely((paramPtr->flags & NSF_ARG_NOCONFIG) != 0u)) {
        continue;
      }

      if (ch1 == paramPtr->name[2]
          && strncmp(nameString, paramPtr->name, length) == 0) {
        Nsf_Param const *pPtr;

        /* fprintf(stderr, "... <%s> is an abbrev of <%s>\n", nameString, paramPtr->name); */
        /*
         * Check, if the abbreviation is unique
         */
        for (pPtr = paramPtr + 1; likely(pPtr->name != NULL) && *pPtr->name == '-'; pPtr++) {
          if (unlikely((pPtr->flags & NSF_ARG_NOCONFIG) != 0u)) {
            continue;
          }
          if (ch1 == pPtr->name[2]
              && strncmp(nameString, pPtr->name, length) == 0) {
            /*
             * The abbreviation is not unique
             */
            *paramPtrPtr = NULL;
            return NsfPrintError(interp, "the provided argument %s is an abbreviation for %s and %s",
                                 nameString, paramPtr->name, pPtr->name);
          }
        }
        /*
         * The abbreviation is unique
         */
        *paramPtrPtr = paramPtr;
        return TCL_OK;
      }
    }
  }
  *paramPtrPtr = NULL;
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NsfProcDeleteProc --
 *
 *    FreeProc for procs with associated parameter definitions.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Freeing memory.
 *
 *----------------------------------------------------------------------
 */
static void
NsfProcDeleteProc(ClientData clientData) {
  NsfProcContext *ctxPtr;

  nonnull_assert(clientData != NULL);

  ctxPtr = (NsfProcContext *)clientData;
  if (ctxPtr->oldDeleteProc != NULL) {
    (*ctxPtr->oldDeleteProc)(ctxPtr->oldDeleteData);
  }
  if (ctxPtr->paramDefs != NULL) {
    /*fprintf(stderr, "free ParamDefs %p\n", ctxPtr->paramDefs);*/
    ParamDefsRefCountDecr(ctxPtr->paramDefs);
  }
  /*fprintf(stderr, "free %p\n", ctxPtr);*/
  FREE(NsfProcContext, ctxPtr);
}

/*
 *----------------------------------------------------------------------
 * ParamDefsStore --
 *
 *    Store the provided parameter definitions in the provided
 *    command. It stores a new deleteProc which will call the original
 *    delete proc automatically.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int ParamDefsStore(Tcl_Interp *interp, Tcl_Command cmd, NsfParamDefs *paramDefs, int checkAlwaysFlag)
  nonnull(1) nonnull(2);

static int
ParamDefsStore(Tcl_Interp *interp, Tcl_Command cmd, NsfParamDefs *paramDefs, int checkAlwaysFlag) {
  Command *cmdPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmd != NULL);

  cmdPtr = (Command *)cmd;
  /*
   * TODO This function might store empty paramDefs. needed?
   */

  if (cmdPtr->deleteProc != NsfProcDeleteProc) {
    NsfProcContext *ctxPtr = NEW(NsfProcContext);

    /*fprintf(stderr, "ParamDefsStore %p (%s) replace deleteProc %p by %p\n",
            paramDefs, Tcl_GetCommandName(interp, cmd),
            cmdPtr->deleteProc, NsfProcDeleteProc);*/

    ctxPtr->oldDeleteData   = (Proc *)cmdPtr->deleteData;
    ctxPtr->oldDeleteProc   = cmdPtr->deleteProc;
    cmdPtr->deleteProc      = NsfProcDeleteProc;
    ctxPtr->paramDefs       = paramDefs;
    ctxPtr->checkAlwaysFlag = checkAlwaysFlag;
    cmdPtr->deleteData      = ctxPtr;

    return TCL_OK;
  } else {
    /*fprintf(stderr, "ParamDefsStore cmd %p has already NsfProcDeleteProc deleteData %p\n",
      cmd, cmdPtr->deleteData);*/
    if (cmdPtr->deleteData != NULL) {
      NsfProcContext *ctxPtr = cmdPtr->deleteData;

      assert(ctxPtr->paramDefs == NULL);
      ctxPtr->paramDefs = paramDefs;
    }
  }
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 * ParamDefsNew --
 *
 *    Allocate a new paramDefs structure and initialize it with zeros. The
 *    allocated structure should be freed with ParamDefsFree().
 *
 * Results:
 *    pointer to paramDefs structure
 *
 * Side effects:
 *    Allocating memory
 *
 *----------------------------------------------------------------------
 */
static NsfParamDefs *
ParamDefsNew() {
  NsfParamDefs *paramDefs;
  static NsfMutex serialMutex = 0;
  static int serial = 0;

  paramDefs = NEW(NsfParamDefs);
  memset(paramDefs, 0, sizeof(NsfParamDefs));

  /* We could keep the serial as well in thread local storage */
  NsfMutexLock(&serialMutex);
  paramDefs->serial = serial++;
  NsfMutexUnlock(&serialMutex);

  /*fprintf(stderr, "ParamDefsNew %p\n", paramDefs);*/

  return paramDefs;
}


/*
 *----------------------------------------------------------------------
 * ParamDefsFree --
 *
 *    Actually free the parameter definitions. Since the parameter definitions
 *    are ref-counted, this function should be just called via
 *    ParamDefsRefCountDecr.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free the parameter definitions.
 *
 *----------------------------------------------------------------------
 */

static void ParamDefsFree(NsfParamDefs *paramDefs) nonnull(1);

static void
ParamDefsFree(NsfParamDefs *paramDefs) {
  /*fprintf(stderr, "ParamDefsFree %p slotObj %p returns %p\n",
    paramDefs, paramDefs->slotObj, paramDefs->returns);*/

  nonnull_assert(paramDefs != NULL);

  if (paramDefs->paramsPtr != NULL) {
    ParamsFree(paramDefs->paramsPtr);
  }
  if (paramDefs->returns != NULL) {DECR_REF_COUNT2("paramDefsObj", paramDefs->returns);}
  FREE(NsfParamDefs, paramDefs);
}

/*
 *----------------------------------------------------------------------
 * ParamDefsRefCountIncr --
 * ParamDefsRefCountDecr --
 *
 *    Perform book keeping on the parameter definitions. RefCounting is
 *    necessary, since it might be possible that during the processing of the
 *    e.g. object parameters, these might be redefined (when an object
 *    parameter calls a method, redefining the
 *    structures). ParamDefsRefCountDecr() is responsible for actually freeing
 *    the structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    No direct.
 *
 *----------------------------------------------------------------------
 */


static void
ParamDefsRefCountIncr(NsfParamDefs *paramDefs) {

  nonnull_assert(paramDefs != NULL);

  paramDefs->refCount ++;
}

static void
ParamDefsRefCountDecr(NsfParamDefs *paramDefs) {

  nonnull_assert(paramDefs != NULL);

  paramDefs->refCount --;
  if (paramDefs->refCount < 1) {
    ParamDefsFree(paramDefs);
  }
}

/*
 *----------------------------------------------------------------------
 * ParamDefsFormatOption --
 *
 *    Append a parameter option to the nameStringObj representing the
 *    syntax of the parameter definition.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static void ParamDefsFormatOption(Tcl_Obj *nameStringObj, const char *option,
                                  int *colonWritten, int *firstOption)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static void
ParamDefsFormatOption(Tcl_Obj *nameStringObj, const char *option,
                      int *colonWritten, int *firstOption) {

  nonnull_assert(nameStringObj != NULL);
  nonnull_assert(option != NULL);
  nonnull_assert(colonWritten != NULL);
  nonnull_assert(firstOption != NULL);

  if (!*colonWritten) {
    Tcl_AppendLimitedToObj(nameStringObj, ":", 1, INT_MAX, NULL);
    *colonWritten = 1;
  }
  if (*firstOption) {
    *firstOption = 0;
  } else {
    Tcl_AppendLimitedToObj(nameStringObj, ",", 1, INT_MAX, NULL);
  }
  Tcl_AppendLimitedToObj(nameStringObj, option, -1, INT_MAX, NULL);
}

/*
 *----------------------------------------------------------------------
 * ParamDefsFormat --
 *
 *    Produce a Tcl_Obj representing a single parameter in the syntax
 *    of the parameter definition.
 *
 * Results:
 *    Tcl_Obj
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *ParamDefsFormat(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern)
  nonnull(1) nonnull(2) returns_nonnull;

static int ParamsDefMatchPattern(Nsf_Param const *paramsPtr, const char *pattern) {
  if (paramsPtr->nameObj != NULL) {
    return Tcl_StringMatch(ObjStr(paramsPtr->nameObj), pattern);
  }
  return Tcl_StringMatch(paramsPtr->name, pattern);
}


static Tcl_Obj *
ParamDefsFormat(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern) {
  int first, colonWritten;
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL), *innerListObj, *nameStringObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramsPtr != NULL);

  INCR_REF_COUNT2("paramDefsObj", listObj);

  for (; likely(paramsPtr->name != NULL); paramsPtr++) {
    if ((paramsPtr->flags & NSF_ARG_NOCONFIG) != 0u) {
      continue;
    }
    if (paramsPtr->paramObj != NULL) {
      if (pattern && !ParamsDefMatchPattern(paramsPtr, pattern)) {
        continue;
      }
      innerListObj = paramsPtr->paramObj;
    } else {
      /*
       * We need this part only for C-defined parameter definitions, defined
       * via genTclAPI.
       *
       * TODO: we could streamline this by defining as well C-API via the same
       * syntax as for accepted for tcl obj types "nsfParam"
       */
      int isNonpos = *paramsPtr->name == '-';
      int outputRequired = (isNonpos && ((paramsPtr->flags & NSF_ARG_REQUIRED) != 0u));
      int outputOptional = (!isNonpos && ((paramsPtr->flags & NSF_ARG_REQUIRED) == 0u)
                            && !paramsPtr->defaultValue &&
                            paramsPtr->converter != ConvertToNothing);
      first = 1;
      colonWritten = 0;

      if (NsfParamDefsAppendVirtual(interp, listObj, paramsPtr, contextObject, pattern, ParamDefsFormat)) {
        continue;
      }
      if (pattern && !ParamsDefMatchPattern(paramsPtr, pattern)) {
        continue;
      }

      nameStringObj = Tcl_NewStringObj(paramsPtr->name, -1);

      if (paramsPtr->type != NULL) {
        ParamDefsFormatOption(nameStringObj, paramsPtr->type, &colonWritten, &first);
      } else if (isNonpos && paramsPtr->nrArgs == 0) {
        ParamDefsFormatOption(nameStringObj, "switch", &colonWritten, &first);
      }
      if (outputRequired != 0) {
        ParamDefsFormatOption(nameStringObj, "required", &colonWritten, &first);
      } else if (outputOptional != 0) {
        ParamDefsFormatOption(nameStringObj, "optional", &colonWritten, &first);
      }
      if ((paramsPtr->flags & NSF_ARG_SUBST_DEFAULT) != 0u) {
        ParamDefsFormatOption(nameStringObj, "substdefault", &colonWritten, &first);
      }
      if ((paramsPtr->flags & NSF_ARG_ALLOW_EMPTY) != 0u || (paramsPtr->flags & NSF_ARG_MULTIVALUED) != 0u) {
        char option[10] = "....";

        option[0] = ((paramsPtr->flags & NSF_ARG_ALLOW_EMPTY) != 0u) ? '0' : '1';
        option[3] = ((paramsPtr->flags & NSF_ARG_MULTIVALUED) != 0u) ? '*' : '1';
        ParamDefsFormatOption(nameStringObj, option, &colonWritten, &first);
      }
      if ((paramsPtr->flags & NSF_ARG_IS_CONVERTER) != 0u) {
        ParamDefsFormatOption(nameStringObj, "convert", &colonWritten, &first);
      }
      if ((paramsPtr->flags & NSF_ARG_INITCMD) != 0u) {
        ParamDefsFormatOption(nameStringObj, "initcmd", &colonWritten, &first);
      } else if ((paramsPtr->flags & NSF_ARG_CMD) != 0u) {
        ParamDefsFormatOption(nameStringObj, "cmd", &colonWritten, &first);
      } else if ((paramsPtr->flags & NSF_ARG_ALIAS) != 0u) {
        ParamDefsFormatOption(nameStringObj, "alias", &colonWritten, &first);
      } else if ((paramsPtr->flags & NSF_ARG_FORWARD) != 0u) {
        ParamDefsFormatOption(nameStringObj, "forward", &colonWritten, &first);
      } else if ((paramsPtr->flags & NSF_ARG_NOARG) != 0u) {
        ParamDefsFormatOption(nameStringObj, "noarg", &colonWritten, &first);
      } else if ((paramsPtr->flags & NSF_ARG_NOCONFIG) != 0u) {
        ParamDefsFormatOption(nameStringObj, "noconfig", &colonWritten, &first);
      }

      innerListObj = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, innerListObj, nameStringObj);
      if (paramsPtr->defaultValue != NULL) {
        Tcl_ListObjAppendElement(interp, innerListObj, paramsPtr->defaultValue);
      }
    }

    Tcl_ListObjAppendElement(interp, listObj, innerListObj);
  }

  return listObj;
}

/*
 *----------------------------------------------------------------------
 * ParamDefsList --
 *
 *    Produce a Tcl_ListObj containing the list of the parameters
 *    based on a parameter structure.
 *
 * Results:
 *    Tcl_Obj
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *ParamDefsList(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern)
  nonnull(1) nonnull(2) returns_nonnull;

static Tcl_Obj *
ParamDefsList(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern) {
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);
  nonnull_assert(paramsPtr != NULL);

  INCR_REF_COUNT2("paramDefsObj", listObj);

  for (; likely(paramsPtr->name != NULL); paramsPtr++) {
    if ((paramsPtr->flags & NSF_ARG_NOCONFIG) != 0u) {
      continue;
    }
    if (NsfParamDefsAppendVirtual(interp, listObj, paramsPtr, contextObject, pattern, ParamDefsList)) {
      continue;
    }

    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(paramsPtr->name, -1));
  }

  return listObj;
}


/*
 *----------------------------------------------------------------------
 * ParamDefsNames --
 *
 *    Produce a Tcl_ListObj containing the names of the parameters
 *    based on a parameter structure.
 *
 * Results:
 *    Tcl_Obj
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj * ParamDefsNames(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern)
  nonnull(1) nonnull(2) returns_nonnull;

static Tcl_Obj *
ParamDefsNames(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern) {
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL), *obj;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramsPtr != NULL);

  INCR_REF_COUNT2("paramDefsObj", listObj);

  for (; likely(paramsPtr->name != NULL); paramsPtr++) {
    if ((paramsPtr->flags & NSF_ARG_NOCONFIG) != 0u) {
      continue;
    }
    if (NsfParamDefsAppendVirtual(interp, listObj, paramsPtr, contextObject, pattern, ParamDefsNames)) {
      continue;
    }

    obj = (paramsPtr->nameObj != NULL) ? paramsPtr->nameObj : Tcl_NewStringObj(paramsPtr->name, -1);
    if (pattern && !Tcl_StringMatch(ObjStr(obj), pattern)) {
      continue;
    }

    Tcl_ListObjAppendElement(interp, listObj, obj);
  }

  return listObj;
}

/*
 *----------------------------------------------------------------------
 * ParamGetType --
 *
 *    Obtain the type of a single parameter and return it as a string.
 *
 * Results:
 *    Type of the parameter in form of a string
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static const char *ParamGetType(Nsf_Param const *paramPtr) nonnull(1) returns_nonnull;

static const char *
ParamGetType(Nsf_Param const *paramPtr) {
  const char *result = "value";

  nonnull_assert(paramPtr != NULL);

  if (paramPtr->type != NULL) {
    if (paramPtr->converter == ConvertViaCmd) {
      result = paramPtr->type + 5;
    } else if (paramPtr->converter == Nsf_ConvertToClass &&
               ((paramPtr->flags & (NSF_ARG_BASECLASS|NSF_ARG_METACLASS)) != 0u) ) {
      if ((paramPtr->flags & NSF_ARG_BASECLASS) != 0u) {
        result = "baseclass";
      } else {
        result = "metaclass";
      }
    } else if (strcmp(paramPtr->type, "stringtype") == 0) {
      if (paramPtr->converterArg != NULL) {
        result = ObjStr(paramPtr->converterArg);
      }
    } else {
      result = paramPtr->type;
    }
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * ParamGetDomain --
 *
 *    Obtain the domain of a single parameter and return it as a
 *    string. The domain is an approximate type used in the parameter
 *    syntax.
 *
 * Results:
 *    Domain of the parameter in form of a string
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static const char * ParamGetDomain(Nsf_Param const *paramPtr) nonnull(1) returns_nonnull;

static const char *
ParamGetDomain(Nsf_Param const *paramPtr) {
  const char *result = "value";

  nonnull_assert(paramPtr != NULL);

  if ((paramPtr->flags & NSF_ARG_IS_ENUMERATION) != 0u) {
    return Nsf_EnumerationTypeGetDomain(paramPtr->converter);
  } else {
    result = ParamGetType(paramPtr);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfParamDefsSyntaxOne --
 *
 *    Appends the formatted parameter (provided as 2nd argument) to the
 *    content of the first argument.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Appending to first argument.
 *
 *----------------------------------------------------------------------
 */

static void NsfParamDefsSyntaxOne(Tcl_Obj *argStringObj, Nsf_Param const *pPtr)
  nonnull(1) nonnull(2);

static void
NsfParamDefsSyntaxOne(Tcl_Obj *argStringObj, Nsf_Param const *pPtr) {

  nonnull_assert(argStringObj != NULL);
  nonnull_assert(pPtr != NULL);

  if (pPtr->nrArgs > 0 && *pPtr->name == '-') {
    Tcl_AppendLimitedToObj(argStringObj, pPtr->name, -1, INT_MAX, NULL);
    Tcl_AppendLimitedToObj(argStringObj, " ", 1, INT_MAX, NULL);
    if ((pPtr->flags & NSF_ARG_IS_ENUMERATION) != 0u) {
      Tcl_AppendLimitedToObj(argStringObj, ParamGetDomain(pPtr), -1, INT_MAX, NULL);
      if ((pPtr->flags & NSF_ARG_MULTIVALUED) != 0u)  {
        Tcl_AppendLimitedToObj(argStringObj, " ...", 4, INT_MAX, NULL);
      }
    } else {
      Tcl_AppendLimitedToObj(argStringObj, "/", 1, INT_MAX, NULL);
      Tcl_AppendLimitedToObj(argStringObj, ParamGetDomain(pPtr), -1, INT_MAX, NULL);
      if ((pPtr->flags & NSF_ARG_MULTIVALUED) != 0u) {
        Tcl_AppendLimitedToObj(argStringObj, " ...", 4, INT_MAX, NULL);
      }
      Tcl_AppendLimitedToObj(argStringObj, "/", 1, INT_MAX, NULL);
    }
  } else if (*pPtr->name != '-') {
    Tcl_AppendLimitedToObj(argStringObj, "/", 1, INT_MAX, NULL);
    Tcl_AppendLimitedToObj(argStringObj, pPtr->name, -1, INT_MAX, NULL);
    Tcl_AppendLimitedToObj(argStringObj, "/", 1, INT_MAX, NULL);
  } else {
    Tcl_AppendLimitedToObj(argStringObj, pPtr->name, -1, INT_MAX, NULL);
  }
}

/*
 * NsfParamDefsVirtualFormat --
 *
 *    This function is called, when we know we can resolve a virtual argument
 *    against the context object. In such cases, obtain the resolved parsed
 *    params and call the formatter.
 *
 * Results:
 *    Standard Tcl result code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
NsfParamDefsVirtualFormat(Tcl_Interp *interp, Nsf_Param const *pPtr, NsfObject *contextObject, const char *pattern,
                          NsfFormatFunction formatFunction) {
  NsfParsedParam parsedParam;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(pPtr != NULL);
  assert(pPtr->type != NULL);
  nonnull_assert(contextObject != NULL);
  nonnull_assert(formatFunction != NULL);

  if (strcmp(pPtr->type, "virtualobjectargs") == 0) {
    result = GetObjectParameterDefinition(interp, NsfGlobalObjs[NSF_EMPTY], contextObject, NULL, &parsedParam);
  } else if (NsfObjectIsClass(contextObject)) {
    result = GetObjectParameterDefinition(interp, NsfGlobalObjs[NSF_EMPTY], NULL, (NsfClass *)contextObject, &parsedParam);
  } else {
    NsfLog(interp, NSF_LOG_WARN, "virtual args: provided context is not a class <%s>", ObjectName(contextObject));
    result = TCL_ERROR;
  }

  if (result == TCL_OK && parsedParam.paramDefs != NULL) {
    return (*formatFunction)(interp, parsedParam.paramDefs->paramsPtr, contextObject, pattern);
  }

  return NULL;
}

/*
 *----------------------------------------------------------------------
 * NsfParamDefsAppendVirtual --
 *
 *    Check for the given paramsPtr wether this is a virtual parameter and if
 *    possible, resolve it and append the formatted content to the Tcl_Obj.
 *
 * Results:
 *    Boolean value for success
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int
NsfParamDefsAppendVirtual(Tcl_Interp *interp, Tcl_Obj *listObj,
                          Nsf_Param const *paramsPtr, NsfObject *contextObject,
                          const char *pattern, NsfFormatFunction formatFunction) {
  nonnull_assert(interp != NULL);
  nonnull_assert(listObj != NULL);
  nonnull_assert(paramsPtr != NULL);
  nonnull_assert(formatFunction != NULL);
  
  assert(paramsPtr->name != NULL);
  
  if (paramsPtr->converter == ConvertToNothing && strcmp(paramsPtr->name, "args") == 0) {

    if ((contextObject != NULL)
        && (paramsPtr->type != NULL)
        && strncmp(paramsPtr->type, "virtual", 7) == 0
        ) {
      Tcl_Obj *formattedObj = NsfParamDefsVirtualFormat(interp, paramsPtr, contextObject, pattern, formatFunction);

      if (formattedObj != NULL) {
        Tcl_ListObjAppendList(interp, listObj, formattedObj);
        DECR_REF_COUNT2("paramDefsObj", formattedObj);

        return 1;
      }
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * NsfParamDefsSyntax --
 *
 *    Return the parameter definitions of a sequence of parameters in
 *    the form of the "parametersyntax", inspired by the Tcl manual
 *    pages.
 *
 * Results:
 *    Tcl_Obj containing the parameter syntax
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *NsfParamDefsSyntax(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern)
  nonnull(1) nonnull(2) returns_nonnull;

Tcl_Obj *
NsfParamDefsSyntax(Tcl_Interp *interp, Nsf_Param const *paramsPtr, NsfObject *contextObject, const char *pattern) {
  Tcl_Obj *argStringObj = Tcl_NewObj();
  Nsf_Param const *pPtr;
  int needSpace = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramsPtr != NULL);

  INCR_REF_COUNT2("paramDefsObj", argStringObj);

  for (pPtr = paramsPtr; pPtr->name; pPtr++) {

    if ((pPtr->flags & NSF_ARG_NOCONFIG) != 0u) {
      /*
       * Don't output non-configurable parameters
       */
      continue;
    }

    if (pPtr != paramsPtr) {
      /*
       * Don't output non-consuming parameters (i.e. positional, and no args)
       */
      if (*pPtr->name != '-' && pPtr->nrArgs == 0) {
        continue;
      }
    }

    if (pPtr->converter == ConvertToNothing && strcmp(pPtr->name, "args") == 0) {
      int argsResolved = 0;

      if ((contextObject != NULL)
          && (pPtr->type != NULL)
          && strncmp(pPtr->type, "virtual", 7) == 0
          ) {
        Tcl_Obj *formattedObj = NsfParamDefsVirtualFormat(interp, pPtr, contextObject,
                                                          pattern, NsfParamDefsSyntax);

        if (formattedObj != NULL) {
          argsResolved = 1;
          if (needSpace != 0) {
            Tcl_AppendLimitedToObj(argStringObj, " ", 1, INT_MAX, NULL);
          }
          Tcl_AppendObjToObj(argStringObj, formattedObj);
          DECR_REF_COUNT2("paramDefsObj", formattedObj);
        }
      }
      if (argsResolved == 0) {
        if (pattern && !ParamsDefMatchPattern(pPtr, pattern)) {
          continue;
        }
        if (needSpace != 0) {
          Tcl_AppendLimitedToObj(argStringObj, " ", 1, INT_MAX, NULL);
        }
        Tcl_AppendLimitedToObj(argStringObj, "?/arg .../?", 11, INT_MAX, NULL);
      }

    } else if ((pPtr->flags & NSF_ARG_REQUIRED) != 0u) {
      if (pattern && !ParamsDefMatchPattern(pPtr, pattern)) {
        continue;
      }
      if (needSpace != 0) {
        Tcl_AppendLimitedToObj(argStringObj, " ", 1, INT_MAX, NULL);
      }

      if ((pPtr->flags & NSF_ARG_IS_ENUMERATION) != 0u) {
        Tcl_AppendLimitedToObj(argStringObj, Nsf_EnumerationTypeGetDomain(pPtr->converter), -1, INT_MAX, NULL);
      } else {
        NsfParamDefsSyntaxOne(argStringObj, pPtr);
      }

    } else {
      if (pattern && !ParamsDefMatchPattern(pPtr, pattern)) {
        continue;
      }
      if (needSpace != 0) Tcl_AppendLimitedToObj(argStringObj, " ", 1, INT_MAX, NULL);
      Tcl_AppendLimitedToObj(argStringObj, "?", 1, INT_MAX, NULL);
      NsfParamDefsSyntaxOne(argStringObj, pPtr);
      Tcl_AppendLimitedToObj(argStringObj, "?", 1, INT_MAX, NULL);
    }
    needSpace = 1;
  }
  /* caller has to decr */
  return argStringObj;
}

/*
 *----------------------------------------------------------------------
 * ParsedParamFree --
 *
 *    Free the provided information of the parsed parameters.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Freed Memory.
 *
 *----------------------------------------------------------------------
 */
static void
ParsedParamFree(NsfParsedParam *parsedParamPtr) {

  nonnull_assert(parsedParamPtr != NULL);

  /*fprintf(stderr, "ParsedParamFree %p, npargs %p\n",
    parsedParamPtr, parsedParamPtr->paramDefs);*/
  if (parsedParamPtr->paramDefs != NULL) {
    ParamDefsRefCountDecr(parsedParamPtr->paramDefs);
  }
  FREE(NsfParsedParam, parsedParamPtr);
}



/*
 * method dispatch
 */
/*
 *----------------------------------------------------------------------
 * ProcMethodDispatchFinalize --
 *
 *    Finalization function for ProcMethodDispatch which executes
 *    scripted methods. Essentially it handles post-assertions and
 *    frees per-invocation memory. The function was developed for NRE
 *    enabled Tcl versions but is used in the same way for non-NRE
 *    enabled versions.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int ProcMethodDispatchFinalize(ClientData data[], Tcl_Interp *interp, int result) nonnull(1) nonnull(2);

static int
ProcMethodDispatchFinalize(ClientData data[], Tcl_Interp *interp, int result) {
  ParseContext *pcPtr = data[0];
  /*const char *methodName = data[2];*/
#if defined(NSF_WITH_ASSERTIONS) || defined(NRE)
  NsfCallStackContent *cscPtr = data[1];
#endif
#if defined(NSF_WITH_ASSERTIONS)
  NsfObject *object = cscPtr->self;
  NsfObjectOpt *opt = object->opt;
#endif

  nonnull_assert(data != NULL);
  nonnull_assert(interp != NULL);

  /*fprintf(stderr, "ProcMethodDispatchFinalize %s %s flags %.6x isNRE %d pcPtr %p result %d\n",
          ObjectName(object), methodName,
          cscPtr->flags, (cscPtr->flags & NSF_CSC_CALL_IS_NRE), pcPtr, result);*/

#if defined(NSF_WITH_ASSERTIONS)
  if (unlikely(opt != NULL && object->teardown != NULL && (opt->checkoptions & CHECK_POST))
      && likely(result == TCL_OK)) {
    int rc = AssertionCheck(interp, object, cscPtr->cl, data[2], CHECK_POST);
    if (rc != TCL_OK) {
      result = rc;
    }
  }
#endif

#if defined(NRE)
  if (likely((cscPtr->flags & NSF_CSC_CALL_IS_NRE) != 0u)) {
    if (likely(pcPtr != NULL)) {
      ParseContextRelease(pcPtr);
      NsfTclStackFree(interp, pcPtr, "release parse context");
    }
    result = ObjectDispatchFinalize(interp, cscPtr, result /*, "NRE" , methodName*/);

    CscFinish(interp, cscPtr, result, "scripted finalize");
  }
#else
  if (unlikely(pcPtr != NULL)) {
    ParseContextRelease(pcPtr);
  }
#endif

  return result;
}

/*
 *----------------------------------------------------------------------
 * ProcDispatchFinalize --
 *
 *    Finalization function for nsf::proc. Simplified version of
 *    ProcMethodDispatchFinalize().
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int ProcDispatchFinalize(ClientData data[], Tcl_Interp *interp, int result) nonnull(1) nonnull(2);

static int
ProcDispatchFinalize(ClientData data[], Tcl_Interp *interp, int result) {
  ParseContext *pcPtr = data[1];

  /*const char *methodName = data[0];
    fprintf(stderr, "ProcDispatchFinalize of method %s\n", methodName);*/

  nonnull_assert(data != NULL);
  nonnull_assert(interp != NULL);

# if defined(NSF_PROFILE)
  long int startUsec = (long int)data[2];
  long int startSec = (long int)data[3];
  char *methodName = data[0];
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  if (rst->doProfile != 0) {
    NsfProfileRecordProcData(interp, methodName, startSec, startUsec);
  }
# endif

  ParseContextRelease(pcPtr);
  NsfTclStackFree(interp, pcPtr, "nsf::proc dispatch finalize release parse context");

  return result;
}


/*
 *----------------------------------------------------------------------
 * ProcMethodDispatch --
 *
 *    Invoke a scripted method (with assertion checking and filters).
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int ProcMethodDispatch(ClientData cp, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
         const char *methodName, NsfObject *object, NsfClass *cl, Tcl_Command cmdPtr,
         NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5) nonnull(6) nonnull(8) nonnull(9);

static int
ProcMethodDispatch(ClientData cp, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
         const char *methodName, NsfObject *object, NsfClass *cl, Tcl_Command cmdPtr,
         NsfCallStackContent *cscPtr) {
  int result, releasePc = 0, checkAlwaysFlag = 0;
  NsfParamDefs *paramDefs;
#if defined(NSF_WITH_ASSERTIONS)
  NsfObjectOpt *opt = object->opt;
#endif
#if defined(NRE)
  ParseContext *pcPtr = NULL;
#else
  ParseContext pc, *pcPtr = &pc;
#endif

  nonnull_assert(cp != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmdPtr != NULL);
  nonnull_assert(cscPtr != NULL);
  nonnull_assert(object != NULL);
  
  assert(object->teardown != NULL);

#if defined(NRE)
  /*fprintf(stderr, "ProcMethodDispatch cmd %s\n", Tcl_GetCommandName(interp, cmdPtr));*/
  assert((cscPtr->flags & NSF_CSC_CALL_IS_NRE) != 0u);
#endif

  /*
   * If this is a filter, check whether its guard applies,
   * if not: just step forward to the next filter
   */

  if (unlikely(cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER)) {
    NsfCmdList *cmdList;
    /*
     * seek cmd in obj's filterOrder
     */
    assert((object->flags & NSF_FILTER_ORDER_VALID) != 0u);
    /* otherwise: FilterComputeDefined(interp, object);*/

    for (cmdList = object->filterOrder; cmdList && cmdList->cmdPtr != cmdPtr; cmdList = cmdList->nextPtr);

    if (cmdList != NULL) {
      /*
       * A filter was found, check whether it has a guard.
       */
      if (cmdList->clientData != NULL) {
        result = GuardCall(object, interp, cmdList->clientData, cscPtr);
      } else {
        result = TCL_OK;
      }

      if (unlikely(result != TCL_OK)) {
        /*fprintf(stderr, "Filter GuardCall in invokeProc returned %d\n", result);*/

        if (likely(result != TCL_ERROR)) {
          /*
           * The guard failed (but no error), and we call "next".
           * Since we may not be in a method with already provided
           * arguments, we call next with the actual arguments and
           * perform no argument substitution.
           *
           * The call stack content is not jet pushed to the Tcl
           * stack, we pass it already to search-and-invoke.
           */

          /*fprintf(stderr, "... calling nextmethod cscPtr %p\n", cscPtr);*/
          result = NextSearchAndInvoke(interp, methodName, objc, objv, cscPtr, 0);
          /*fprintf(stderr, "... after nextmethod result %d\n", result);*/
        }

        /*
         * Next might have succeeded or not, but we are done. In the
         * NRE-case, we need a CscFinish for all return codes.
         */
#if defined(NRE)
        CscFinish(interp, cscPtr, result, "guard failed");
#endif
        return result;
      }
    }
  }

#if defined(NSF_WITH_ASSERTIONS)
  if (unlikely(opt != NULL && (opt->checkoptions & CHECK_PRE)) &&
      (result = AssertionCheck(interp, object, cl, methodName, CHECK_PRE)) == TCL_ERROR) {
    goto prep_done;
  }
#endif

  /*
   *  If the method to be invoked has paramDefs, we have to call the
   *  argument parser with the argument definitions obtained from the
   *  proc context from the cmdPtr.
   */
  paramDefs = ParamDefsGet(cmdPtr, &checkAlwaysFlag);

  if (paramDefs && paramDefs->paramsPtr) {
#if defined(NRE)
    pcPtr = (ParseContext *) NsfTclStackAlloc(interp, sizeof(ParseContext), "parse context");
#endif
    result = ProcessMethodArguments(pcPtr, interp, object,
                                    checkAlwaysFlag|NSF_ARGPARSE_METHOD_PUSH|NSF_ARGPARSE_FORCE_REQUIRED,
                                    paramDefs, objv[0], objc, objv);
    cscPtr->objc = objc;
    cscPtr->objv = (Tcl_Obj **)objv;

    if (likely(result == TCL_OK)) {
      releasePc = 1;
      result = PushProcCallFrame(cp, interp, pcPtr->objc+1, pcPtr->full_objv, cscPtr);
    } else {
      /*
       * some error occurred
       */
#if defined(NRE)
      ParseContextRelease(pcPtr);
      NsfTclStackFree(interp, pcPtr, "parse context (proc prep failed)");
      pcPtr = NULL;
#else
      ParseContextRelease(pcPtr);
#endif
    }
  } else {
    result = PushProcCallFrame(cp, interp, objc, objv, cscPtr);
  }

  /*
   * The stack frame is pushed, we could do something here before
   * running the byte code of the body.
   */

  /* we could consider to run here ARG_METHOD or ARG_INITCMD
  if (likely(result == TCL_OK)) {

  }
  */

#if defined(NSF_WITH_ASSERTIONS)
 prep_done:
#endif

  if (likely(result == TCL_OK)) {
#if defined(NRE)
    /*fprintf(stderr, "CALL TclNRInterpProcCore %s method '%s'\n",
      ObjectName(object), ObjStr(objv[0]));*/
    Tcl_NRAddCallback(interp, ProcMethodDispatchFinalize,
                      (releasePc != 0) ? pcPtr : NULL,
                      cscPtr,
                      (ClientData)methodName,
                      NULL);
    cscPtr->flags |= NSF_CSC_CALL_IS_NRE;
    result = TclNRInterpProcCore(interp, objv[0], 1, &MakeProcError);
#else
    ClientData data[3] = {
        releasePc ? pcPtr : NULL,
        cscPtr,
        (ClientData)methodName
    };

    result = TclObjInterpProcCore(interp, objv[0], 1, &MakeProcError);
    result = ProcMethodDispatchFinalize(data, interp, result);
#endif
  } else /* result != OK */ {
#if defined(NRE)
    CscFinish(interp, cscPtr, result, "nre, prep failed");
#endif
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * CmdMethodDispatch --
 *
 *    Invoke a method implemented as a cmd.  Essentially it stacks
 *    optionally a frame, calls the method, pops the frame and runs
 *    invariants.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Indirect effects by calling cmd
 *
 *----------------------------------------------------------------------
 */
static int CmdMethodDispatch(ClientData cp, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                             NsfObject *object, Tcl_Command cmd, NsfCallStackContent *cscPtr)
  nonnull(2) nonnull(4) nonnull(5) nonnull(6);

static int
CmdMethodDispatch(ClientData cp, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                  NsfObject *object, Tcl_Command cmd,
                  NsfCallStackContent *cscPtr) {
  CallFrame frame, *framePtr = &frame;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(object != NULL);
  
  assert(object->teardown != NULL);

#if defined(NRE)
  assert(!cscPtr || (cscPtr->flags & NSF_CSC_CALL_IS_NRE) == 0u);
#endif

  if (cscPtr != NULL) {
    /*
     * We have a call-stack content, but the requested dispatch will not store
     * the call-stack content in a corresponding call-frame on its own. To get,
     * for example, self introspection working for the requested dispatch, we
     * introduce a CMETHOD frame.
     */
    /*fprintf(stderr, "Nsf_PushFrameCsc %s %s\n", ObjectName(object), Tcl_GetCommandName(interp, cmd));*/
    Nsf_PushFrameCsc(interp, cscPtr, framePtr);
    result = Tcl_NRCallObjProc(interp, Tcl_Command_objProc(cmd), cp, objc, objv);
    Nsf_PopFrameCsc(interp, framePtr);
  } else {
    result = Tcl_NRCallObjProc(interp, Tcl_Command_objProc(cmd), cp, objc, objv);
  }

#if defined(NSF_WITH_ASSERTIONS)
  if (unlikely(object->opt != NULL) && likely(result == TCL_OK)) {
    CheckOptions co = object->opt->checkoptions;
    if ((co & CHECK_INVAR)) {
      int rc = AssertionCheckInvars(interp, object, Tcl_GetCommandName(interp, cmd), co);

      if (rc != TCL_OK) {
        result = rc;
      }
    }
  }
#endif

  /*
   * Reference counting in the calling ObjectDispatch() makes sure
   * that obj->opt is still accessible even after "dealloc"
   */
  return result;
}

/*
 *----------------------------------------------------------------------
 * ObjectCmdMethodDispatch --
 *
 *    Invoke a method implemented as an object. The referenced object is used
 *    as a source for methods to be executed.  Essentially this is currently
 *    primarily used to implement the dispatch of ensemble objects.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Indirect effects by calling cmd
 *
 *----------------------------------------------------------------------
 */

static int ObjectCmdMethodDispatch(NsfObject *invokedObject, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                        const char *methodName, NsfObject *callerSelf, NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5) nonnull(6) nonnull(7);

static int
ObjectCmdMethodDispatch(NsfObject *invokedObject, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                        const char *methodName, NsfObject *callerSelf, NsfCallStackContent *cscPtr) {
  CallFrame frame, *framePtr = &frame;
  Tcl_Command cmd, subMethodCmd;
  const char *subMethodName;
  NsfObject *actualSelf;
  NsfClass *actualClass;
  int result;

  nonnull_assert(invokedObject != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(callerSelf != NULL);
  nonnull_assert(cscPtr != NULL);

  cmd = cscPtr->cmdPtr;
  /*fprintf(stderr, "ObjectCmdMethodDispatch %p %s\n", cmd, Tcl_GetCommandName(interp, cmd));*/
  /*fprintf(stderr, "ObjectCmdMethodDispatch method %s invokedObject %p %s callerSelf %p %s\n",
          methodName, invokedObject, ObjectName(invokedObject),
          callerSelf, ObjectName(callerSelf));*/

  if (unlikely((invokedObject->flags & NSF_DELETED) != 0u)) {
    /*
     * When we try to invoke a deleted object, the cmd (alias) is
     * automatically removed. Note that the cmd might be still referenced
     * in various entries in the call-stack. The reference counting on
     * these elements takes care that the cmdPtr is deleted on a pop
     * operation (although we do a Tcl_DeleteCommandFromToken() below.
     */

    /*fprintf(stderr, "methodName %s found DELETED object with cmd %p my cscPtr %p\n",
      methodName, cmd, cscPtr);*/

    Tcl_DeleteCommandFromToken(interp, cmd);
    if (cscPtr->cl != NULL) {
      NsfInstanceMethodEpochIncr("DeleteObjectAlias");
    } else {
      NsfObjectMethodEpochIncr("DeleteObjectAlias");
    }

    NsfCleanupObject(invokedObject, "alias-delete1");
    return NsfPrintError(interp, "trying to dispatch deleted object via method '%s'",
                         methodName);
  }

  /*
   * Check, if the object cmd was called without a reference to a method. If
   * so, perform the standard dispatch of default methods.
   */
  if (unlikely(objc < 2)) {

    if ((invokedObject->flags & NSF_PER_OBJECT_DISPATCH) != 0u) {
      cscPtr->flags |= NSF_CSC_CALL_IS_ENSEMBLE;
    }
    Nsf_PushFrameCsc(interp, cscPtr, framePtr);
    result = DispatchDefaultMethod(interp, invokedObject, objv[0], NSF_CSC_IMMEDIATE);
    Nsf_PopFrameCsc(interp, framePtr);
    return result;
  }

  /*
   * Check, if we want NSF_KEEP_CALLER_SELF. The setting of this flag
   * determines the values of actualSelf and actualClass.
   */
  if ((invokedObject->flags & NSF_KEEP_CALLER_SELF) != 0u) {
    actualSelf = callerSelf;
    actualClass = cscPtr->cl;
  } else {
    actualSelf = invokedObject;
    actualClass = NULL;
  }
  subMethodName = ObjStr(objv[1]);

  if ((invokedObject->flags & NSF_PER_OBJECT_DISPATCH) == 0u) {
    /*fprintf(stderr, "invokedObject %p %s methodName %s: no perobjectdispatch\n",
      invokedObject, ObjectName(invokedObject), methodName);*/
#if 0
    /*
     * We should have either an approach
     *  - to obtain from an object to methodname the cmd, and
     *    call e.g. MethodDispatch(), or pass a fully qualified
     *     method name, or
     *  - to pass an the actualSelf and invokedObject both
     *    to MethodDispatch/MethodDispatch
     *  TODO: maybe remove NSF_CM_KEEP_CALLER_SELF when done.
     */
    result = MethodDispatch(object, interp,
                            nobjc+1, nobjv-1, cmd, object,
                            NULL /*NsfClass *cl*/,
                            Tcl_GetCommandName(interp, cmd),
                            NSF_CSC_TYPE_PLAIN, flags);
#endif
#if 1
    /* simple and brutal */
    if (likely(invokedObject->nsPtr != NULL)) {
      subMethodCmd = FindMethod(invokedObject->nsPtr, subMethodName);
    } else {
      subMethodCmd = NULL;
    }

    if (subMethodCmd == NULL) {
      /* no -system handling */
      actualClass = SearchPLMethod(invokedObject->cl->order, subMethodName, &subMethodCmd,
                                   NSF_CMD_CALL_PRIVATE_METHOD);
    }
    if (likely(subMethodCmd != NULL)) {
      cscPtr->objc = objc;
      cscPtr->objv = objv;
      Nsf_PushFrameCsc(interp, cscPtr, framePtr);
      result = MethodDispatch(actualSelf,
                              interp, objc-1, objv+1,
                              subMethodCmd, actualSelf, actualClass, subMethodName,
                              cscPtr->frameType|NSF_CSC_TYPE_ENSEMBLE,
                              (cscPtr->flags & 0xFF)|NSF_CSC_IMMEDIATE);
      Nsf_PopFrameCsc(interp, framePtr);
      return result;
    }

  /*fprintf(stderr, "... objv[0] %s cmd %p %s csc %p\n",
    ObjStr(objv[0]), subMethodCmd, subMethodName, cscPtr); */

    if (0) {
      fprintf(stderr, "new unknown\n");
      return DispatchUnknownMethod(interp, invokedObject, /* objc-1, objv+1*/ objc, objv, actualSelf->cmdName,
                                   objv[1], NSF_CM_NO_OBJECT_METHOD|NSF_CSC_IMMEDIATE);
  }
#endif
    return ObjectDispatch(actualSelf, interp, objc, objv, NSF_CM_KEEP_CALLER_SELF);
  }

  /*
   * NSF_PER_OBJECT_DISPATCH is set
   */

  if (likely(invokedObject->nsPtr != NULL)) {
    subMethodCmd = FindMethod(invokedObject->nsPtr, subMethodName);
  } else {
    subMethodCmd = NULL;
  }

  /*
   * Make sure, that the current call is marked as an ensemble call, both
   * for dispatching to the default-method and for dispatching the method
   * interface of the given object. Otherwise, current introspection
   * specific to sub-methods fails (e.g., a [current method-path] in the
   * default-method).
   */
  cscPtr->flags |= NSF_CSC_CALL_IS_ENSEMBLE;

  /* fprintf(stderr, "ensemble dispatch cp %s %s objc %d\n",
     ObjectName((NsfObject*)cp), methodName, objc);*/

  cscPtr->objc = objc;
  cscPtr->objv = objv;
  Nsf_PushFrameCsc(interp, cscPtr, framePtr);

  /*fprintf(stderr, "... objv[0] %s cmd %p %s csc %p\n",
    ObjStr(objv[0]), subMethodCmd, subMethodName, cscPtr); */

  if (likely(subMethodCmd != NULL)) {
    /*
     * In order to allow [next] to be called in an ensemble method,
     * an extra call-frame is needed. This CSC frame is typed as
     * NSF_CSC_TYPE_ENSEMBLE. Note that the associated call is flagged
     * additionally (NSF_CSC_CALL_IS_ENSEMBLE; see above) to be able
     * to identify ensemble-specific frames during [next] execution.
     *
     * The dispatch requires NSF_CSC_IMMEDIATE to be set, ensuring
     * that scripted methods are executed before the ensemble ends. If
     * they were executed later, they would find their parent frame
     * (CMETHOD) being popped from the stack already.
     */

    /*fprintf(stderr, ".... ensemble dispatch object %s self %s pass %s\n",
      ObjectName(invokedObject), ObjectName(actualSelf), (actualSelf->flags & NSF_KEEP_CALLER_SELF) ? "callerSelf" : "invokedObject");
      fprintf(stderr, ".... ensemble dispatch on %s.%s objflags %.8x cscPtr %p base flags %.6x flags %.6x cl %s\n",
      ObjectName(actualSelf), subMethodName, actualSelf->flags,
      cscPtr, (0xFF & cscPtr->flags), (cscPtr->flags & 0xFF)|NSF_CSC_IMMEDIATE, (actualClass != NULL) ? ClassName(actualClass) : "NONE");*/
    result = MethodDispatch(actualSelf,
                            interp, objc-1, objv+1,
                            subMethodCmd, actualSelf, actualClass, subMethodName,
                            cscPtr->frameType|NSF_CSC_TYPE_ENSEMBLE,
                            (cscPtr->flags & 0xFF)|NSF_CSC_IMMEDIATE);
    /*if (unlikely(result != TCL_OK)) {
      fprintf(stderr, "ERROR: cmd %p %s subMethodName %s -- %s -- %s\n",
      subMethodCmd, Tcl_GetCommandName(interp, subMethodCmd), subMethodName,
      Tcl_GetCommandName(interp, cscPtr->cmdPtr), ObjStr(Tcl_GetObjResult(interp)));
      }*/

  } else {
    /*
     * The method to be called was not part of this ensemble. Call
     * next to try to call such methods along the next path.
     */
    Tcl_CallFrame *framePtr1;
    NsfCallStackContent *cscPtr1 = CallStackGetTopFrame(interp, &framePtr1);

    /*fprintf(stderr, "call next instead of unknown %s.%s \n",
      ObjectName(cscPtr->self), methodName);*/

    assert(cscPtr1 != NULL);
    if ((cscPtr1->frameType & NSF_CSC_TYPE_ENSEMBLE)) {
      /*
       * We are in an ensemble method. The next works here not on the
       * actual methodName + frame, but on the ensemble above it. We
       * locate the appropriate call-stack content and continue next on
       * that.
       */
      cscPtr1 = CallStackFindEnsembleCsc(framePtr1, &framePtr1);
      assert(cscPtr1 != NULL);
    }

    /*
     * We mark in the flags that we are in an ensemble but failed so far to
     * resolve the cmd. Now we try to resolve the unknown subcmd via next and
     * we record this in the flags. The method name for next might be
     * colon-prefixed. In these cases, we have to skip the single colon with
     * the MethodName() function.
     */
    cscPtr1-> flags |= NSF_CM_ENSEMBLE_UNKNOWN;
    /* fprintf(stderr, "==> trying to find <%s> in ensemble <%s> via next\n",
       subMethodName, MethodName(cscPtr1->objv[0]));*/
    result = NextSearchAndInvoke(interp, MethodName(cscPtr1->objv[0]),
                                 cscPtr1->objc, cscPtr1->objv, cscPtr1, 0);

    /*fprintf(stderr, "==> next %s.%s subMethodName %s (obj %s) cscPtr %p (flags %.8x)) cscPtr1 %p (flags %.8x) result %d unknown %d\n",
            ObjectName(callerSelf), methodName, subMethodName, ObjectName(invokedObject),
            cscPtr, cscPtr->flags, cscPtr1, (cscPtr1 != NULL) ? cscPtr1->flags : 0,
            result, RUNTIME_STATE(interp)->unknown);*/

    if (RUNTIME_STATE(interp)->unknown) {
      Tcl_Obj *callInfoObj = Tcl_NewListObj(1, &callerSelf->cmdName);
      Tcl_CallFrame *varFramePtr, *tclFramePtr = CallStackGetTclFrame(interp,(Tcl_CallFrame *)framePtr, 1);
      int pathLength, pathLength0 = 0, getPath = 1, unknownIndex;
      Tcl_Obj *pathObj = NsfMethodNamePath(interp, tclFramePtr, MethodName(objv[0]));

      /*
       * The "next" call could not resolve the unknown subcommand. At this
       * point, potentially serval different ensembles were tried, which can
       * be found on the stack.
       *
       * Example1:  call: foo a b d
       *    mixin:  foo a b c
       *    object: foo a x
       *
       * We want to return the longest, most precise prefix (here "foo a b")
       * and flag "d" as unknown (here the mixin frame). Another (inferior)
       * solution would be to report "foo a" as know prefix and "b d" as
       * unknown (when the error is generated from the point of view of the
       * object method frame).
       *
       * In the general case, we traverse the stack for all ensembles and pick
       * the longest known ensemble for reporting. This path is passed to the
       * unknown-handler of the ensemble.
       */

      Tcl_ListObjLength(interp, pathObj, &pathLength0);
      pathLength = pathLength0;

      for (varFramePtr = (Tcl_CallFrame *)framePtr; likely(varFramePtr != NULL);
           varFramePtr = Tcl_CallFrame_callerVarPtr(varFramePtr)) {
        NsfCallStackContent *cscPtr;

        /*
         * If we reach a non-nsf frame, or it is not an ensemble, we are done.
         */
        cscPtr = (Tcl_CallFrame_isProcCallFrame(varFramePtr) & (FRAME_IS_NSF_METHOD|FRAME_IS_NSF_CMETHOD)) ?
          (NsfCallStackContent *)Tcl_CallFrame_clientData(varFramePtr) : NULL;
        if (cscPtr == NULL || (cscPtr->flags & NSF_CSC_CALL_IS_ENSEMBLE) == 0u) {
          break;
        }
        /*
         * Every ensemble block starts with a frame of
         * NSF_CSC_TYPE_ENSEMBLE. If we find one, then we compute a new path
         * in the next iteration.
         */
        if ((cscPtr->frameType & (NSF_CSC_TYPE_ENSEMBLE)) == 0) {
          /* get method path the next round */
          getPath = 1;
        } else if (getPath == 1) {
          Tcl_Obj *pathObj1 = CallStackMethodPath(interp, varFramePtr);
          int pathLength1;

          getPath = 0;
          Tcl_ListObjLength(interp, pathObj1, &pathLength1);
          if (pathLength1 > pathLength) {
            if (pathObj != NULL) {
              DECR_REF_COUNT(pathObj);
            }
            pathObj    = pathObj1;
            pathLength = pathLength1;
          }
        }
      }

      unknownIndex = pathLength <= pathLength0 ? 1 : 1 + pathLength - pathLength0;
      assert(objc > unknownIndex);

      INCR_REF_COUNT(callInfoObj);
      Tcl_ListObjAppendList(interp, callInfoObj, pathObj);
      Tcl_ListObjAppendElement(interp, callInfoObj, objv[unknownIndex]);

      /* fprintf(stderr, "DispatchUnknownMethod is called with callinfo <%s> (callerSelf <%s>, methodName '%s', methodPath '%s')\n",
              ObjStr(callInfoObj), ObjStr(callerSelf->cmdName), MethodName(objv[0]),
              ObjStr(callInfoObj)); */
      result = DispatchUnknownMethod(interp, invokedObject, objc-1, objv+1, callInfoObj,
                                     objv[1], NSF_CM_NO_OBJECT_METHOD|NSF_CSC_IMMEDIATE);
      DECR_REF_COUNT(callInfoObj);
    }
  }
  Nsf_PopFrameCsc(interp, framePtr);

  return result;
}

#if !defined(NSF_ASSEMBLE)
static int NsfAsmProc(ClientData clientData, Tcl_Interp *interp,
                      int objc, Tcl_Obj *CONST objv[]) {
  return TCL_OK;
}
#endif


/*
 *----------------------------------------------------------------------
 * CheckCStack --
 *
 *    Monitor the growth of the C Stack when complied with
 *    NSF_STACKCHECK.
 *
 * Results:
 *    none
 *
 * Side effects:
 *    update of rst->bottomOfStack
 *
 *----------------------------------------------------------------------
 */
#if defined(NSF_STACKCHECK)
NSF_INLINE static void CheckCStack(Tcl_Interp *interp, const char *prefix, const char *fullMethodName)
  nonnull(1) nonnull(2) nonnull(3);

NSF_INLINE static void
CheckCStack(Tcl_Interp *interp, const char *prefix, const char *fullMethodName) {
  int somevar;
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(prefix != NULL);
  nonnull_assert(fullMethodName != NULL);

  if (rst->exitHandlerDestroyRound == NSF_EXITHANDLER_OFF) {
# if TCL_STACK_GROWS_UP
    if ((void *)&somevar < rst->bottomOfStack) {
      NsfLog(interp, NSF_LOG_WARN, "Stack adjust bottom %ld - %s %s",
             (void *)&somevar - rst->bottomOfStack, prefix, fullMethodName);
      rst->bottomOfStack = (void *)&somevar;
    } else if ((void *)&somevar > rst->maxStack) {
      NsfLog(interp, NSF_LOG_WARN, "Stack adjust top %ld - %s %s",
             (void *)&somevar - rst->bottomOfStack, prefix, fullMethodName);
      rst->maxStack = (void *)&somevar;
    }
# else
    if ((void *)&somevar > rst->bottomOfStack) {
      NsfLog(interp, NSF_LOG_WARN, "Stack adjust bottom %ld - %s %s",
             rst->bottomOfStack - (void *)&somevar, prefix, fullMethodName);
      rst->bottomOfStack = (void *)&somevar;
    } else if ((void *)&somevar < rst->maxStack) {
      NsfLog(interp, NSF_LOG_WARN, "Stack adjust top %ld - %s %s",
             rst->bottomOfStack - (void *)&somevar, prefix, fullMethodName);
      rst->maxStack = (void *)&somevar;
    }
# endif
  }
}
#else
# define CheckCStack(interp, prefix, methodName)
#endif

/*
 *----------------------------------------------------------------------
 * MethodDispatchCsc --
 *
 *    Dispatch a method (scripted or cmd) with an already allocated
 *    call stack content. The method calls either ProcMethodDispatch()
 *    (for scripted methods) or CmdMethodDispatch() (otherwise).
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Indirect effects by calling methods
 *
 *----------------------------------------------------------------------
 */
static int MethodDispatchCsc(ClientData clientData, Tcl_Interp *interp,
                  int objc, Tcl_Obj *CONST objv[],
                  Tcl_Command cmd,
                  NsfCallStackContent *cscPtr, const char *methodName,
                  int *validCscPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5) nonnull(6) nonnull(7) nonnull(8);

static int
MethodDispatchCsc(ClientData clientData, Tcl_Interp *interp,
                  int objc, Tcl_Obj *CONST objv[],
                  Tcl_Command cmd,
                  NsfCallStackContent *cscPtr, const char *methodName,
                  int *validCscPtr) {
  NsfObject *object;
  ClientData cp = Tcl_Command_objClientData(cmd);
  Tcl_ObjCmdProc *proc = Tcl_Command_objProc(cmd);
  NsfCallStackContent *cscPtr1;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(cscPtr != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(validCscPtr != NULL);

  object = cscPtr->self;
  /*
   * Privide DTrace with calling info
   */
  if (NSF_DTRACE_METHOD_ENTRY_ENABLED()) {
    NSF_DTRACE_METHOD_ENTRY(ObjectName(object), (cscPtr->cl != NULL) ? ClassName(cscPtr->cl) : ObjectName(object),
                            (char *)methodName,
                            objc-1, (Tcl_Obj **)objv+1);
  }

  /*fprintf(stderr, "MethodDispatch method '%s' cmd %p %s clientData %p cp=%p objc=%d cscPtr %p csc->flags %.6x \n",
          methodName, cmd, Tcl_GetCommandName(interp, cmd), clientData,
          cp, objc, cscPtr, cscPtr->flags);*/
  /*fprintf(stderr, "MethodDispatch method '%s' cmd %p cp=%p objc=%d cscPtr %p csc->flags %.6x "
          "obj->flags %.6x teardown %p\n",
          methodName, cmd, cp, objc, cscPtr, cscPtr->flags, object->flags, object->teardown);*/
  assert(object->teardown != NULL);

  /*
   * The default assumption is that the CscPtr is valid after this function
   * finishes.
   */

  if (likely(proc == TclObjInterpProc)) {
    int result;
#if defined(NRE)
    NRE_callback *rootPtr = TOP_CB(interp);
    int isImmediate = (cscPtr->flags & NSF_CSC_IMMEDIATE);
# if defined(NRE_CALLBACK_TRACE)
    NsfClass *cl = cscPtr->cl;
# endif
#endif
    /*
     * The cmd is a scripted method
     */

    result = ProcMethodDispatch(cp, interp, objc, objv, methodName,
                                  object, cscPtr->cl, cmd, cscPtr);
#if defined(NRE)
    /*
     * In the NRE case, there is no trust in the cscPtr anymore, it might be already gone.
     */
    *validCscPtr = 0;

    if (unlikely(isImmediate)) {
# if defined(NRE_CALLBACK_TRACE)
      fprintf(stderr, ".... manual run callbacks rootPtr = %p, result %d methodName %s.%s\n",
              rootPtr, result, (cl != NULL) ?ClassName(cl):"NULL", methodName);
# endif
      result = NsfNRRunCallbacks(interp, result, rootPtr);
    } else {
# if defined(NRE_CALLBACK_TRACE)
      fprintf(stderr, ".... don't run callbacks rootPtr = %p, result %d methodName %s.%s\n",
              rootPtr, result, (cl != NULL) ?ClassName(cl):"NULL", methodName);
# endif
    }
#endif
    /*
     * scripted method done
     */
    return result;

  } else if (proc == NsfObjDispatch) {

    assert(cp != NULL);
    return ObjectCmdMethodDispatch((NsfObject *)cp, interp, objc, objv,
                                   methodName, object, cscPtr);

  } else if (cp != NULL) {

    cscPtr1 = cscPtr;

    /*fprintf(stderr, "cscPtr %p cmd %p %s wanna stack cmd %p %s cp %p no-leaf %d force frame %d\n",
            cscPtr, cmd, Tcl_GetCommandName(interp, cmd),
            cmd, Tcl_GetCommandName(interp, cmd),
            cp,
            (Tcl_Command_flags(cmd) & NSF_CMD_NONLEAF_METHOD),
            (cscPtr->flags & NSF_CSC_FORCE_FRAME));*/
    /*
     * The cmd has client data, we check for required updates in this
     * structure.
     */

    if (proc == NsfForwardMethod ||
        proc == NsfObjscopedMethod ||
        proc == NsfSetterMethod ||
        proc == NsfAsmProc
        ) {
      TclCmdClientData *tcd = (TclCmdClientData *)cp;

      assert(tcd != NULL);
      tcd->object = object;
      assert((CmdIsProc(cmd) == 0));

    } else if (cp == (ClientData)NSF_CMD_NONLEAF_METHOD) {
      cp = clientData;
      assert((CmdIsProc(cmd) == 0));

    }
#if !defined(NDEBUG)
    else if (proc == NsfProcAliasMethod) {
      /* This should never happen */
      Tcl_Panic("Alias invoked in unexpected way");
    }
#endif


  } else if ((Tcl_Command_flags(cmd) & NSF_CMD_NONLEAF_METHOD)
             || ((cscPtr->flags & NSF_CSC_FORCE_FRAME) != 0u)) {
    /*
     * Technically, we would not need a frame to execute the cmd, but maybe,
     * the user want's it (to be able to call next, or the keep proc-level
     * variables. The clientData cp is in such cases typically NULL.
     */
    /*fprintf(stderr, "FORCE_FRAME\n");*/
    cscPtr1 = cscPtr;

  } else {
    /*
     * There is no need to pass a frame. Use the original clientData.
     */
    cscPtr1 = NULL;
  }

  if (cscPtr1 != NULL) {
    /*
     * Call with a stack frame.
     */

    /*fprintf(stderr, "cmdMethodDispatch %s.%s, cscPtr %p objflags %.6x\n",
      ObjectName(object), methodName, cscPtr, object->flags); */

    return CmdMethodDispatch(cp, interp, objc, objv, object, cmd, cscPtr1);
  } else {
    /*
     * Call without a stack frame.
     */
    CscListAdd(interp, cscPtr);

    /*fprintf(stderr, "cmdMethodDispatch %p %s.%s, nothing stacked, objflags %.6x\n",
      cmd, ObjectName(object), methodName, object->flags); */

    return CmdMethodDispatch(clientData, interp, objc, objv, object, cmd, NULL);
  }
}

/*
 *----------------------------------------------------------------------
 * MethodDispatch --
 *
 *    Convenience wrapper for MethodDispatchCsc(). It allocates a call
 *    stack content and invokes MethodDispatchCsc.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Indirect effects by calling methods
 *
 *----------------------------------------------------------------------
 */
static int
MethodDispatch(ClientData clientData, Tcl_Interp *interp,
               int objc, Tcl_Obj *CONST objv[],
               Tcl_Command cmd, NsfObject *object, NsfClass *cl,
               const char *methodName, int frameType, unsigned int flags) {
  NsfCallStackContent csc, *cscPtr;
  int result, isValidCsc = 1;
  Tcl_Command resolvedCmd;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  
  assert(object->teardown != NULL);

  CheckCStack(interp, "method", methodName);

  /*fprintf(stderr, "MethodDispatch method '%s.%s' objc %d flags %.6x\n",
    ObjectName(object), methodName, objc, flags); */

  resolvedCmd = AliasDereference(interp, object, methodName, cmd);
  if (unlikely(resolvedCmd == NULL)) {
    return TCL_ERROR;
  }

  /*
   * cscAlloc uses for resolvedCmd for allocating the call stack content and
   * sets the IS_NRE flag based on it. We use the original cmd in the
   * callstack content structure for introspection.
   */
  cscPtr = CscAlloc(interp, &csc, resolvedCmd);

  /*
   * We would not need CscInit when cp (clientData) == NULL &&
   * !(Tcl_Command_flags(cmd) & NSF_CMD_NONLEAF_METHOD) TODO: We could pass
   * cmd == NULL, but is this worth it?
   */
  CscInit(cscPtr, object, cl, cmd, frameType, flags, methodName);

  result = MethodDispatchCsc(object, interp, objc, objv,
                             resolvedCmd, cscPtr, methodName, &isValidCsc);

#if defined(NRE)
  if (isValidCsc == 1) {
    CscListRemove(interp, cscPtr, NULL);
    CscFinish(interp, cscPtr, result, "csc cleanup");
  }
#else
  CscListRemove(interp, cscPtr, NULL);
  CscFinish(interp, cscPtr, result, "csc cleanup");
#endif

  return result;
}

/*
 *----------------------------------------------------------------------
 * ObjectDispatchFinalize --
 *
 *    Finalization function for ObjectDispatch() which performs method
 *    lookup and call all kind of methods. The function runs after
 *    ObjectDispatch() and calls the unknown handler if necessary and
 *    resets the filter and mixin stacks.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe side effects by the cmd called by ParameterCheck()
 *    or DispatchUnknownMethod()
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int ObjectDispatchFinalize(Tcl_Interp *interp, NsfCallStackContent *cscPtr,
                                             int result /*, char *msg, const char *methodName*/)
  nonnull(1) nonnull(2);

NSF_INLINE static int
ObjectDispatchFinalize(Tcl_Interp *interp, NsfCallStackContent *cscPtr,
                       int result /*, char *msg, const char *methodName*/) {
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  NsfObject *object;
  unsigned int flags;

  nonnull_assert(interp != NULL);
  nonnull_assert(cscPtr != NULL);
  
  object = cscPtr->self;
  assert(object != NULL);
  assert(object->id != NULL);

  flags = cscPtr->flags;

  /*fprintf(stderr, "ObjectDispatchFinalize %p %s flags %.6x (%d) frame %.6x unk %d m %s\n",
          cscPtr, ObjectName(object), flags,
          result, cscPtr->frameType, RUNTIME_STATE(interp)->unknown, (cscPtr->cmdPtr != NULL) ? Tcl_GetCommandName(interp, cscPtr->cmdPtr) : "");*/

  /*
   * Check the return value if wanted
   */
  if (likely(result == TCL_OK
             && (cscPtr->cmdPtr != NULL)
             && (Tcl_Command_cmdEpoch(cscPtr->cmdPtr) == 0))) {
    NsfParamDefs *paramDefs = ParamDefsGet(cscPtr->cmdPtr, NULL);

    if ((paramDefs != NULL) && paramDefs->returns) {
      Tcl_Obj *valueObj = Tcl_GetObjResult(interp);

      result = ParameterCheck(interp, paramDefs->returns, valueObj, "return-value:",
                              rst->doCheckResults, 0, 0, NULL);
    }
  } else {
    /*fprintf(stderr, "We have no cmdPtr in cscPtr %p %s",  cscPtr, ObjectName(object));
    fprintf(stderr, "... cannot check return values!\n");*/
  }


  /*
   * On success (no error occurred) check for unknown cases.
   */
  if (likely(result == TCL_OK)) {

    if (unlikely(((flags & NSF_CSC_METHOD_IS_UNKNOWN) != 0u)
                 || ((cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER) && rst->unknown)
                 )) {
      result = DispatchUnknownMethod(interp, object,
                                     cscPtr->objc, cscPtr->objv, NULL, cscPtr->objv[0],
                                     (cscPtr->flags & NSF_CSC_CALL_NO_UNKNOWN)|NSF_CSC_IMMEDIATE);
    }
  }

  /*
   * Resetting mixin and filter stacks
   */

  if (unlikely((flags & NSF_CSC_MIXIN_STACK_PUSHED) && object->mixinStack) != 0u) {
    /* fprintf(stderr, "MixinStackPop %s.%s %p %s\n",
       ObjectName(object), methodName, object->mixinStack, msg);*/
    MixinStackPop(object);
  }
  if (unlikely((flags & NSF_CSC_FILTER_STACK_PUSHED) && object->filterStack) != 0u) {
    /* fprintf(stderr, "FilterStackPop %s.%s %p %s\n",
       ObjectName(object), methodName, object->filterStack, msg);*/
    FilterStackPop(object);
  }

  return result;
}

/*#define INHERIT_CLASS_METHODS 1*/

#if defined(INHERIT_CLASS_METHODS)
static Tcl_Command NsfFindClassMethod(Tcl_Interp *interp, NsfClass *cl, const char *methodName)
  nonnull(1) nonnull(2) nonnull(3);

static Tcl_Command
NsfFindClassMethod(Tcl_Interp *interp, NsfClass *cl, const char *methodName) {
  Tcl_Command cmd;
  NsfClasses *p;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(methodName != NULL);

  /*fprintf(stderr, "NsfFindClassMethod %s %s\n", ClassName(cl), methodName);*/
  for(p = PrecedenceOrder(cl); p; p = p->nextPtr) {
    NsfClass *currentClass = p->cl;
    Tcl_Namespace *nsPtr = currentClass->object.nsPtr;

    /*fprintf(stderr, "1 check for obj ns in class %s => %p\n",
      ClassName(currentClass), nsPtr);*/
    if (nsPtr != NULL) {
      cmd = FindMethod(nsPtr, methodName);
      /*fprintf(stderr, "1 lookup for method %s in class %s => %p\n",
        methodName, ClassName(currentClass), cmd);*/
      if (cmd != NULL) {return cmd;}
    }
  }
  return NULL;
}
#endif

/*
 *----------------------------------------------------------------------
 * ObjectDispatch --
 *
 *    This function performs the method lookup and call all kind of
 *    methods. It checks, whether a filter or mixin has to be
 *    applied. in these cases, the effective method lookup is
 *    performed by next.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe side effects by the cmd called by ParameterCheck()
 *    or DispatchUnknownMethod()
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int ObjectDispatch(ClientData clientData, Tcl_Interp *interp,
                                     int objc, Tcl_Obj *CONST objv[],
                                     unsigned int flags)
  nonnull(2) nonnull(4);

NSF_INLINE static int
ObjectDispatch(ClientData clientData, Tcl_Interp *interp,
               int objc, Tcl_Obj *CONST objv[],
               unsigned int flags) {
  register NsfObject *object = (NsfObject *)clientData;
  int result = TCL_OK, objflags, shift,
    frameType = NSF_CSC_TYPE_PLAIN;
  const char *methodName;
  NsfObject *calledObject;
  NsfClass *cl = NULL;
  Tcl_Command cmd = NULL;
  Tcl_Obj *cmdName = object->cmdName, *methodObj;
  NsfCallStackContent csc, *cscPtr = NULL;
  int isValidCsc = 1;
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  /* none of the higher copy-flags must be passed */
  assert((flags & (NSF_CSC_COPY_FLAGS & 0x000FFF000U)) == 0u);

  if (unlikely((flags & NSF_CM_NO_SHIFT) != 0u)) {
    shift = 0;
    methodObj = objv[0];
    methodName = MethodName(methodObj);
  } else {
    assert(objc > 1);
    shift = 1;
    methodObj = objv[1];
    methodName = ObjStr(methodObj);
    if (unlikely(FOR_COLON_RESOLVER(methodName))) {
      return NsfPrintError(interp, "%s: method name '%s' must not start with a colon",
                           ObjectName(object), methodName);
    }
  }
  assert(object->teardown != NULL);

#if defined(METHOD_OBJECT_TRACE)
  fprintf(stderr, "method %p/%d '%s' type %p <%s>\n",
          methodObj, methodObj->refCount, methodName, methodObj->typePtr, (methodObj->typePtr != NULL) ? methodObj->typePtr->name : "");
#endif

  /*fprintf(stderr, "ObjectDispatch obj = %s objc = %d 0=%s methodName=%s shift %d\n", (object != NULL) ? ObjectName(object) : NULL,
          objc, objv[0] ? ObjStr(objv[0]) : NULL,
          methodName, shift);*/

  objflags = object->flags; /* avoid stalling */

  /*
   * Make sure, cmdName and obj survive this method until the end of
   * this function.
   */
  INCR_REF_COUNT(cmdName);
  NsfObjectRefCountIncr(object);

  /*fprintf(stderr, "obj refCount of %p after incr %d (ObjectDispatch) %s\n",
    object, object->refCount, methodName);*/

  if (unlikely((objflags & NSF_FILTER_ORDER_VALID) == 0u)) {
    FilterComputeDefined(interp, object);
    objflags = object->flags;
  }

  if (unlikely((objflags & NSF_MIXIN_ORDER_VALID) == 0u)) {
    MixinComputeDefined(interp, object);
    objflags = object->flags;
  }

  /*
   * Only start new filter chain, if
   *   (a) filters are defined and
   *   (b) the toplevel csc entry is not an filter on self
   */

  /*fprintf(stderr, "call %s, objflags %.6x, defined and valid %.6x doFilters %d guard count %d\n",
          methodName, objflags, NSF_FILTER_ORDER_DEFINED_AND_VALID,
          rst->doFilters, rst->guardCount);*/

  assert((flags & (NSF_CSC_MIXIN_STACK_PUSHED|NSF_CSC_FILTER_STACK_PUSHED)) == 0u);

  if (unlikely((objflags & NSF_FILTER_ORDER_DEFINED_AND_VALID) == NSF_FILTER_ORDER_DEFINED_AND_VALID)) {
    if (rst->doFilters && !rst->guardCount) {
      NsfCallStackContent *cscPtr1 = CallStackGetTopFrame0(interp);

      if (cscPtr1 == NULL ||
          (object != cscPtr1->self || (cscPtr1->frameType != NSF_CSC_TYPE_ACTIVE_FILTER))) {
        FilterStackPush(object, methodObj);
        flags |= NSF_CSC_FILTER_STACK_PUSHED;

        cmd = FilterSearchProc(interp, object, &object->filterStack->currentCmdPtr, &cl);
        if (cmd != NULL) {
          /*fprintf(stderr, "*** filterSearchProc returned cmd %p\n", cmd);*/
          frameType = NSF_CSC_TYPE_ACTIVE_FILTER;
          methodName = (char *)Tcl_GetCommandName(interp, cmd);
          flags |= NSF_CM_IGNORE_PERMISSIONS;
        }
      }
    }
  }

  if (unlikely(cmd == NULL && ((flags & NSF_CM_LOCAL_METHOD) != 0u))) {
    /*
     * We require a local method. If the local method is found, we set always
     * the cmd and sometimes the class (if it is a class specific method).
     */
    NsfCallStackContent *cscPtr1 = CallStackGetTopFrame0(interp);

    if (unlikely(cscPtr1 == NULL)) {
      return NsfPrintError(interp, "flag '-local' only allowed when called from a method body");
    }
    if (cscPtr1->cl != NULL) {
      cmd = FindMethod(cscPtr1->cl->nsPtr, methodName);
      if (cmd != NULL) {
        cl = cscPtr1->cl;
      }
    } else if (object->nsPtr != NULL) {
      cmd = FindMethod(object->nsPtr, methodName);
    }

    /*fprintf(stderr, "ObjectDispatch NSF_CM_LOCAL_METHOD obj %s methodName %s => cl %p %s cmd %p \n", (object != NULL) ? ObjectName(object) : NULL,
            methodName, cl, (cl != NULL) ? ClassName(cl) : "NONE", cmd);*/

  } else if (unlikely(*methodName == ':')) {
    NsfObject *regObject;
    int fromClassNS = 0;

    /*
     * We have fully qualified name provided. Determine the class and/or
     * object on which the method was registered.
     */

    INCR_REF_COUNT(methodObj);
    cmd = ResolveMethodName(interp, NULL, methodObj,
                            NULL, &regObject, NULL, NULL, &fromClassNS);
    DECR_REF_COUNT(methodObj);

    if (likely(cmd != NULL)) {
      if (CmdIsNsfObject(cmd)) {
        /*
         * Don't allow for calling objects as methods via fully qualified
         * names. Otherwise, in line [2] below, ::State (or any children of
         * it, e.g., ::Slot::child) is interpreted as a method candidate. As a
         * result, dispatch chaining occurs with ::State or ::State::child
         * being the receiver (instead of Class) of the method call
         * "-parameter". In such a dispatch chaining, the method "unknown"
         * won't be called on Class (in the XOTcl tradition), effectively
         * bypassing any unknown-based indirection mechanism (e.g., XOTcl's short-cutting
         * of object/class creations).
         *
         *  [1] Class ::State; Class ::State::child
         *  [2] Class ::State -parameter x; Class ::State::child -parameter x
         */
        NsfLog(interp, NSF_LOG_NOTICE,
               "Don't invoke object %s this way. Register object via alias ...",
               methodName);
        cmd = NULL;
      } else {
        if (regObject != NULL) {
          if (NsfObjectIsClass(regObject)) {
            cl = (NsfClass *)regObject;
          }
        }
        /* fprintf(stderr, "fully qualified lookup of %s returned %p\n", ObjStr(methodObj), cmd); */
        /* ignore permissions for fully qualified method names */
        flags |= NSF_CM_IGNORE_PERMISSIONS;
      }
      /*fprintf(stderr, "ObjectDispatch fully qualified obj %s methodName %s => cl %p cmd %p \n", (object != NULL) ? ObjectName(object) : NULL,
        methodName, cl, cmd);*/
    }
  }

  /*fprintf(stderr, "MixinStackPush check for %p %s.%s objflags %.6x == %d\n",
    object, ObjectName(object), methodName, objflags & NSF_MIXIN_ORDER_DEFINED_AND_VALID,
    (objflags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) == NSF_MIXIN_ORDER_DEFINED_AND_VALID);*/
  /*
   * Check if a mixed in method has to be called.
   */
  if (unlikely((objflags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) == NSF_MIXIN_ORDER_DEFINED_AND_VALID
               && (flags & (NSF_CM_SYSTEM_METHOD|NSF_CM_INTRINSIC_METHOD)) == 0u
               && ((flags & NSF_CM_LOCAL_METHOD) == 0u || cl != NULL))) {

    /*
     * The current logic allocates first an entry on the per-object
     * stack and searches then for a mixin. This could be improved by
     * allocating a stack entry just when an mixin is found. The same
     * holds for the filters above, but there, the hit-rate is much
     * larger.
     */

    MixinStackPush(object);
    flags |= NSF_CSC_MIXIN_STACK_PUSHED;

    if (frameType != NSF_CSC_TYPE_ACTIVE_FILTER) {
      Tcl_Command cmd1 = cmd;
      /*
       * The entry is just searched and pushed on the stack when we
       * have no filter; in the filter case, the search happens in
       * next.
       */
      result = MixinSearchProc(interp, object, methodName, methodObj, &cl,
                               &object->mixinStack->currentCmdPtr, &cmd1);
      if (unlikely(result != TCL_OK)) {
        /*fprintf(stderr, "mixinsearch returned an error for %p %s.%s\n",
          object, ObjectName(object), methodName);*/
        isValidCsc = 0;
        goto exit_object_dispatch;
      }
      if (cmd1 != NULL) {
        frameType = NSF_CSC_TYPE_ACTIVE_MIXIN;
        cmd = cmd1;
      }
    }
  }

  /*
   * If no fully qualified method name/filter/mixin was found then perform
   * ordinary method lookup. First, try to resolve the method name as a
   * per-object method.
   */

  if (likely(cmd == NULL)) {
    NsfMethodContext *mcPtr = methodObj->internalRep.twoPtrValue.ptr1;
    int nsfObjectMethodEpoch = rst->objectMethodEpoch;

    if (methodObj->typePtr == &NsfObjectMethodObjType
        && mcPtr->context == object
        && mcPtr->methodEpoch == nsfObjectMethodEpoch
        && mcPtr->flags == flags
        ) {
      cmd = mcPtr->cmd;

#if defined(METHOD_OBJECT_TRACE)
      fprintf(stderr, "... use internal rep method %p %s cmd %p (objProc %p) cl %p %s\n",
              methodObj, ObjStr(methodObj),
              cmd, (cmd != NULL) ? ((Command *)cmd)->objProc : 0,
              cl, (cl != NULL) ? ClassName(cl) : ObjectName(object));
#endif

      assert((cmd != NULL) ? ((Command *)cmd)->objProc != NULL : 1);
    } else {
      /*
       * Do we have an object-specific cmd?
       */
      if (unlikely((object->nsPtr != NULL)
                   && (flags & (NSF_CM_NO_OBJECT_METHOD|NSF_CM_SYSTEM_METHOD)) == 0u)) {
        cmd = FindMethod(object->nsPtr, methodName);
        /*fprintf(stderr, "lookup for per-object method in obj %p method %s nsPtr %p"
                " => %p objProc %p\n",
                object, methodName, object->nsPtr, cmd, (cmd != NULL) ? ((Command *)cmd)->objProc : NULL);*/
        if (cmd != NULL) {
          /*
           * Reject call when
           * a) trying to call a private method without the local flag or ignore permssions, or
           * b) trying to call an object with no method interface
           */
          if (((flags & (NSF_CM_LOCAL_METHOD|NSF_CM_IGNORE_PERMISSIONS)) == 0u
               && (Tcl_Command_flags(cmd) & NSF_CMD_CALL_PRIVATE_METHOD))
              ) {
            cmd = NULL;
          } else {

            NsfMethodObjSet(interp, methodObj, &NsfObjectMethodObjType,
                            object, nsfObjectMethodEpoch,
                            cmd, NULL, flags);
          }
        }
      }
    }
#if defined(INHERIT_CLASS_METHODS)
    /* this is not optimized yet, since current class might be checked twice,
       but easier to maintain */
    if ((flags & NSF_CM_NO_OBJECT_METHOD) == 0u && cmd == NULL && NsfObjectIsClass(object)) {
      cmd = NsfFindClassMethod(interp, (NsfClass *)object, methodName);
    }
#endif

    if (likely(cmd == NULL)) {
      /* check for an instance method */
      NsfClass *currentClass = object->cl;
      NsfMethodContext *mcPtr = methodObj->internalRep.twoPtrValue.ptr1;
      int nsfInstanceMethodEpoch = rst->instanceMethodEpoch;

#if defined(METHOD_OBJECT_TRACE)
      fprintf(stderr, "... method %p/%d '%s' type? %d context? %d nsfMethodEpoch %d => %d\n",
              methodObj, methodObj->refCount, ObjStr(methodObj),
              methodObj->typePtr == &NsfInstanceMethodObjType,
              methodObj->typePtr == &NsfInstanceMethodObjType ? mcPtr->context == currentClass : 0,
              methodObj->typePtr == &NsfInstanceMethodObjType ? mcPtr->methodEpoch : 0,
              nsfInstanceMethodEpoch );
#endif

      if (methodObj->typePtr == &NsfInstanceMethodObjType
          && mcPtr->context == currentClass
          && mcPtr->methodEpoch == nsfInstanceMethodEpoch
          && mcPtr->flags == flags
          ) {
        cmd = mcPtr->cmd;
        cl = mcPtr->cl;
#if defined(METHOD_OBJECT_TRACE)
        fprintf(stderr, "... use internal rep method %p %s cmd %p (objProc %p) cl %p %s\n",
                methodObj, ObjStr(methodObj),
                cmd, (cmd != NULL) ?((Command *)cmd)->objProc : NULL,
                cl, (cl != NULL) ? ClassName(cl) : ObjectName(object));
#endif
        assert((cmd != NULL) ? ((Command *)cmd)->objProc != NULL : 1);
      } else {

        /*
         * We could call PrecedenceOrder(currentClass) to recompute
         * currentClass->order on demand, but by construction this is already
         * set here.
         */
        assert(currentClass->order);

        if (unlikely((flags & NSF_CM_SYSTEM_METHOD) != 0u)) {
          NsfClasses *classListPtr = currentClass->order;
          /*
           * Skip entries until the first base class.
           */
          do {
            if (IsBaseClass(&classListPtr->cl->object)) {break;}
            classListPtr = classListPtr->nextPtr;
          } while (classListPtr->nextPtr != NULL);

          cl = SearchPLMethod(classListPtr, methodName, &cmd, NSF_CMD_CALL_PRIVATE_METHOD);
        } else {
          cl = SearchPLMethod(currentClass->order, methodName, &cmd, NSF_CMD_CALL_PRIVATE_METHOD);
        }
        if (methodObj->typePtr != Nsf_OT_tclCmdNameType
            && methodObj->typePtr != Nsf_OT_parsedVarNameType
            ) {
          NsfMethodObjSet(interp, methodObj, &NsfInstanceMethodObjType,
                          currentClass, nsfInstanceMethodEpoch,
                          cmd, cl, flags);
        }
      }
    }
  }
  calledObject = object;

  /*
   * If we have a command, check the permissions, unless
   * NSF_CM_IGNORE_PERMISSIONS is set. Note, that NSF_CM_IGNORE_PERMISSIONS is
   * set currently for fully qualified cmd names and in nsf::object::dispatch.
   */

  if (likely(cmd && (flags & NSF_CM_IGNORE_PERMISSIONS) == 0u)) {
    int cmdFlags = Tcl_Command_flags(cmd);

#if !defined(NDEBUG)
    if (unlikely(((cmdFlags & NSF_CMD_CALL_PRIVATE_METHOD) != 0u)
                 && ((flags & NSF_CM_LOCAL_METHOD) == 0u))
        ) {
      /*
       * Private methods can be only called with the "-local" flag. All cases
       * handling private methods should be covered above (e.g. by setting
       * NSF_CM_IGNORE_PERMISSIONS, or by filtering private methods in method
       * search. So, this branch should never by executed.
       */

      Tcl_Panic("Unexpected handling of private method; most likely a caching bug");
      cmd = NULL;

    } else
#endif
    if (unlikely((cmdFlags & NSF_CMD_CALL_PROTECTED_METHOD) != 0u)) {
      NsfObject *lastSelf = GetSelfObj(interp);

      /*
       * Protected methods can be called, when calling object == called object.
       */

      if (unlikely(object != lastSelf)) {
        NsfLog(interp, NSF_LOG_WARN, "'%s %s' fails since method %s.%s is protected",
               ObjectName(object), methodName, (cl != NULL) ? ClassName(cl) : ObjectName(object), methodName);
        /* reset cmd, since it is still unknown */
        cmd = NULL;
      }
    }
  }

  assert(result == TCL_OK);

  if (likely(cmd != NULL)) {
    /*
     * We found the method to dispatch.
     */
    Tcl_Command resolvedCmd = AliasDereference(interp, object, methodName, cmd);
    if (unlikely(resolvedCmd == NULL)) {
      isValidCsc = 0;
      goto exit_object_dispatch;
    }

    /*
     * cscAlloc uses for resolvedCmd for allocating the call stack content and
     * sets the IS_NRE flag based on it. We use the original cmd in the
     * callstack content structure for introspection.
     */

    cscPtr = CscAlloc(interp, &csc, resolvedCmd);
    CscInit(cscPtr, calledObject, cl, cmd, frameType, flags, methodName);

    if (unlikely(cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER)) {
      /* run filters is not NRE enabled */
      cscPtr->flags |= NSF_CSC_IMMEDIATE;
      /*
       * Setting cscPtr->objc and cscPtr->objv is needed for invoking UNKNOWN
       * from ProcMethodDispatchFinalize()
       */
      cscPtr->objc = objc-shift;
      cscPtr->objv = objv+shift;
    }

    /*fprintf(stderr, "MethodDispatchCsc %s.%s %p flags %.6x cscPtr %p\n",
            ObjectName(object), methodName, object->mixinStack, cscPtr->flags,
            cscPtr);*/

    result = MethodDispatchCsc(clientData, interp, objc-shift, objv+shift,
                               resolvedCmd, cscPtr, methodName, &isValidCsc);

    if (unlikely(result == TCL_ERROR)) {
      /*fprintf(stderr, "Call ErrInProc cl = %p, cmd %p, methodName %s flags %.6x\n",
              cl, (cl != NULL) ? cl->object.id : NULL, methodName, (cl != NULL) ? cl->object.flags : 0);*/

      result = NsfErrInProc(interp, cmdName,
                            cl && cl->object.teardown ? cl->object.cmdName : NULL,
                            methodName);
    }
  } else {
    /*
     * The method to be dispatched is unknown
     */
    cscPtr = CscAlloc(interp, &csc, cmd);
    CscInit(cscPtr, object, cl, cmd, frameType, flags, methodName);
    cscPtr->flags |= NSF_CSC_METHOD_IS_UNKNOWN;
    if ((flags & NSF_CM_NO_UNKNOWN) != 0u) {
      cscPtr->flags |= NSF_CSC_CALL_NO_UNKNOWN;
    }
    cscPtr->objc = objc-shift;
    cscPtr->objv = objv+shift;
  }

 exit_object_dispatch:
  if (likely(isValidCsc == 1)) {
    /*
     * In every situation, we have a cscPtr containing all context information
     */
    assert(cscPtr != NULL);

    result = ObjectDispatchFinalize(interp, cscPtr, result /*, "immediate" , methodName*/);
    CscListRemove(interp, cscPtr, NULL);
    CscFinish(interp, cscPtr, result, "non-scripted finalize");
  }

  /*fprintf(stderr, "ObjectDispatch %s.%s returns %d\n",
    ObjectName(object), methodName, result);*/

  NsfCleanupObject(object, "ObjectDispatch");
  /*fprintf(stderr, "ObjectDispatch call NsfCleanupObject %p DONE\n", object);*/
  DECR_REF_COUNT(cmdName); /* must be after last dereferencing of obj */

  return result;
}

/*
 *----------------------------------------------------------------------
 * DispatchDefaultMethod --
 *
 *    Dispatch the default method (when object is called without arguments)
 *    in case the object system has it defined.
 *
 * Results:
 *    result code.
 *
 * Side effects:
 *    indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int
DispatchDefaultMethod(Tcl_Interp *interp, NsfObject *object,
                      Tcl_Obj *obj, unsigned int flags) {
  int result;
  Tcl_Obj *methodObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(obj != NULL);

  if (CallDirectly(interp, object, NSF_o_defaultmethod_idx, &methodObj)) {

    Tcl_SetObjResult(interp, object->cmdName);
    result = TCL_OK;

  } else {
    Tcl_Obj *tov[2];

    tov[0] = obj;
    tov[1] = methodObj;
    result = ObjectDispatch(object, interp, 2, tov,
                            flags|NSF_CM_NO_UNKNOWN|NSF_CM_IGNORE_PERMISSIONS);
  }

  return result;
}


/*
 *----------------------------------------------------------------------
 * DispatchDestroyMethod --
 *
 *    Dispatch the method "destroy" in case the object system has it
 *    defined. During the final cleanup of the object system, the
 *    destroy is called separately from deallocation. Normally,
 *    Object.destroy() calls dealloc, which is responsible for the
 *    physical deallocation.
 *
 * Results:
 *    result code
 *
 * Side effects:
 *    indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */

static int
DispatchDestroyMethod(Tcl_Interp *interp, NsfObject *object, unsigned int flags) {
  int result;
  Tcl_Obj *methodObj;
  NsfRuntimeState *rst;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  rst = RUNTIME_STATE(interp);
  /*
   * Don't call destroy after exit handler started physical
   * destruction, or when it was called already before
   */
  if (rst->exitHandlerDestroyRound == NSF_EXITHANDLER_ON_PHYSICAL_DESTROY
      || (object->flags & NSF_DESTROY_CALLED) != 0u
      ) {
    return TCL_OK;
  }

  /*fprintf(stderr, "    DispatchDestroyMethod obj %p flags %.6x active %d\n",
    object, object->flags,  object->activationCount); */

  PRINTOBJ("DispatchDestroyMethod", object);

  /* flag, that destroy was called and invoke the method */
  object->flags |= NSF_DESTROY_CALLED;

  if (CallDirectly(interp, object, NSF_o_destroy_idx, &methodObj)) {
    NSF_PROFILE_TIME_DATA;
    NSF_PROFILE_CALL(interp, object, Nsf_SystemMethodOpts[NSF_o_destroy_idx]);
    result = NsfODestroyMethod(interp, object);
    NSF_PROFILE_EXIT(interp, object, Nsf_SystemMethodOpts[NSF_o_destroy_idx]);

  } else {
    result = CallMethod(object, interp, methodObj, 2, NULL,
                        NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE|flags);
  }
  if (unlikely(result != TCL_OK)) {
    /*
     * The object might be already gone here, since we have no stack frame.
     * Therefore, we can't even use nsf::current object safely.
     */
    NsfErrorContext(interp, "method destroy");

    if (++rst->errorCount > 20) {
      Tcl_Panic("too many destroy errors occurred. Endless loop?");
    }
  } else if (rst->errorCount > 0) {
    rst->errorCount--;
  }

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "DispatchDestroyMethod for %p exit\n", object);
#endif
  return result;
}

/*
 *----------------------------------------------------------------------
 * DispatchInitMethod --
 *
in case the object system has it
 *    defined and it was not already called on the object,
 *
 * Results:
 *    Result code.
 *
 * Side effects:
 *    Indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int DispatchInitMethod(Tcl_Interp *interp, NsfObject *object,
                   int objc, Tcl_Obj *CONST objv[], unsigned int flags)
  nonnull(1) nonnull(2);

static int
DispatchInitMethod(Tcl_Interp *interp, NsfObject *object,
                   int objc, Tcl_Obj *CONST objv[], unsigned int flags) {
  int result;
  Tcl_Obj *methodObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*
   * check, whether init was called already
   */
  if ((object->flags & (NSF_INIT_CALLED|NSF_DESTROY_CALLED)) == 0u) {

    /*
     * Flag the call to "init" before the dispatch, such that a call to
     * "configure" within init does not clear the already set instance
     * variables.
     */

    object->flags |= NSF_INIT_CALLED;

    if (CallDirectly(interp, object, NSF_o_init_idx, &methodObj)) {
      /*fprintf(stderr, "%s init directly\n", ObjectName(object));*/
      /*
       * Actually, nothing to do.
       */
      result = TCL_OK;
    } else {
      result = CallMethod(object, interp, methodObj, objc+2, objv,
                          flags|NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE);
    }

  } else {
    result = TCL_OK;
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * DispatchUnknownMethod --
 *
 *    Dispatch the method "unknown" in case the object system has it
 *    defined and the application program contains an unknown handler.
 *
 * Results:
 *    result code
 *
 * Side effects:
 *    There might be indirect effects by calling Tcl code; also,
 *    the interp's unknown-state is reset.
 *
 *----------------------------------------------------------------------
 */

static int
DispatchUnknownMethod(Tcl_Interp *interp, NsfObject *object,
                      int objc, Tcl_Obj *CONST objv[],
                      Tcl_Obj *callInfoObj, Tcl_Obj *methodObj, unsigned int flags) {
  int result;
  Tcl_Obj *unknownObj = NsfMethodObj(object, NSF_o_unknown_idx);
  const char *methodName = MethodName(methodObj);
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(methodObj != NULL);

  /*fprintf(stderr, "compare unknownObj %p with methodObj %p '%s' %p %p %s -- %s\n",
    unknownObj, methodObj, ObjStr(methodObj), callInfoObj, (callInfoObj != NULL) ?objv[1]:NULL, (callInfoObj != NULL) ?ObjStr(objv[1]) : NULL,
    methodName);*/

  if (unknownObj && methodObj != unknownObj && (flags & NSF_CSC_CALL_NO_UNKNOWN) == 0u) {
    /*
     * back off and try unknown;
     */
    int mustCopy = *(ObjStr(methodObj)) == ':';
    ALLOC_ON_STACK(Tcl_Obj*, objc+3, tov);

    if (callInfoObj == NULL) {
      callInfoObj = (mustCopy != 0) ? Tcl_NewStringObj(methodName, -1) : methodObj;
    }
    INCR_REF_COUNT(callInfoObj);

    /*fprintf(stderr, "calling unknown for %s %s, flags=%.6x,%.6x/%.6x isClass=%d %p %s objc %d\n",
      ObjectName(object), ObjStr(methodObj), flags, NSF_CM_NO_UNKNOWN, NSF_CSC_CALL_NO_UNKNOWN,
      NsfObjectIsClass(object), object, ObjectName(object), objc);*/

    tov[0] = object->cmdName;
    tov[1] = unknownObj;
    tov[2] = callInfoObj;
    if (objc > 1) {
      memcpy(tov + 3, objv + 1, sizeof(Tcl_Obj *) * (objc - 1));
    }

    flags &= ~NSF_CM_NO_SHIFT;

    /*fprintf(stderr, "call unknown via dispatch mustCopy %d delegator %p method %s (%s)\n",
      mustCopy, delegator, ObjStr(tov[offset]), ObjStr(methodObj));*/

    result = ObjectDispatch(object, interp, objc+2, tov,
                            flags|NSF_CM_NO_UNKNOWN|NSF_CM_IGNORE_PERMISSIONS);

    DECR_REF_COUNT(callInfoObj);
    FREE_ON_STACK(Tcl_Obj*, tov);

  } else { /* no unknown called, this is the built-in unknown handler */
    Tcl_Obj *tailMethodObj = NULL;

    if (objc > 1 && ((*methodName) == '-' || (unknownObj && objv[0] == unknownObj))) {
      int length;
      if (Tcl_ListObjLength(interp, objv[1], &length) == TCL_OK && length > 0) {
        Tcl_ListObjIndex(interp, objv[1], length - 1, &tailMethodObj);
      }
    }

    result = NsfPrintError(interp, "%s: unable to dispatch method '%s'",
                           ObjectName(object), (tailMethodObj != NULL) ? MethodName(tailMethodObj) : methodName);
  }

  /*
   * Reset interp state, unknown has been fired.
   */
  rst->unknown = 0;

  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfObjDispatch --
 *
 *    This function is called on every object dispatch (when an object
 *    is invoked). It calls either the passed method, or dispatches
 *    some default method.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe side effects by the cmd called by ParameterCheck()
 *    or DispatchUnknownMethod()
 *
 *----------------------------------------------------------------------
 */
#if defined(NRE)
Tcl_ObjCmdProc NsfObjDispatchNRE;
int
NsfObjDispatch(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  return Tcl_NRCallObjProc(interp, NsfObjDispatchNRE, clientData, objc, objv);
}
int NsfObjDispatchNRE(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

int
NsfObjDispatchNRE(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {

#else

EXTERN int
NsfObjDispatch(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {

#endif
  int result;
#ifdef STACK_TRACE
  NsfStackDump(interp);
#endif

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  if (likely(objc > 1)) {
    /*
     * Normal dispatch; we must not use NSF_CSC_IMMEDIATE here,
     * otherwise coroutines won't work.
     */
    result = ObjectDispatch(clientData, interp, objc, objv, 0);
  } else {
    result = DispatchDefaultMethod(interp, (NsfObject *)clientData, objv[0], NSF_CSC_IMMEDIATE);
  }
  return result;
}

/*
 *  Proc-Creation
 */

/*
 *----------------------------------------------------------------------
 * AddPrefixToBody --
 *
 *    Create a fresh TclObj* containing the body with an potential prefix.
 *    The caller has to decrement the ref-count on this Tcl_Obj*.
 *
 * Results:
 *    Tcl_Obj
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj * AddPrefixToBody(Tcl_Obj *body, int paramDefs, NsfParsedParam *paramPtr) nonnull(1) nonnull(3);

static Tcl_Obj *
AddPrefixToBody(Tcl_Obj *body, int paramDefs, NsfParsedParam *paramPtr) {
  Tcl_Obj *resultBody = Tcl_NewObj();

  nonnull_assert(body != NULL);
  nonnull_assert(paramPtr != NULL);

  INCR_REF_COUNT2("resultBody", resultBody);

  if (paramDefs && paramPtr->possibleUnknowns > 0) {
    Tcl_AppendStringsToObj(resultBody, "::nsf::__unset_unknown_args\n", (char *) NULL);
  }

  Tcl_AppendStringsToObj(resultBody, ObjStr(body), (char *) NULL);
  return resultBody;
}

NSF_INLINE static int NoMetaChars(const char *pattern) nonnull(1);

NSF_INLINE static int
NoMetaChars(const char *pattern) {
  register char c;

  nonnull_assert(pattern != NULL);

  for (c = *pattern; c; c = *++pattern) {
    if (c == '*' || c == '?' || c == '[') {
      return 0;
    }
  }
  return 1;
}

/***********************************************************************
 * Nsf_TypeConverter
 ***********************************************************************/

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToString --
 *
 *    Minimal Nsf_TypeConverter setting the client data (passed to C
 *    functions) to the ObjStr of the object.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToString(Tcl_Interp *UNUSED(interp), Tcl_Obj *objPtr, Nsf_Param const *UNUSED(pPtr),
                        ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(2) nonnull(4) nonnull(5);

int
Nsf_ConvertToString(Tcl_Interp *UNUSED(interp), Tcl_Obj *objPtr, Nsf_Param const *UNUSED(pPtr),
                    ClientData *clientData, Tcl_Obj **outObjPtr) {

  nonnull_assert(objPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  *clientData = (char *)ObjStr(objPtr);
  assert(*outObjPtr == objPtr);
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ConvertToNothing --
 *
 *    Minimalistic Nsf_TypeConverter, even setting the client data (passed to
 *    C functions).
 *
 * Results:
 *    Tcl result code, **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int ConvertToNothing(Tcl_Interp *UNUSED(interp), Tcl_Obj *objPtr,  Nsf_Param const *UNUSED(pPtr),
                            ClientData *UNUSED(clientData), Tcl_Obj **outObjPtr)
  nonnull(2) nonnull(5);

static int
ConvertToNothing(Tcl_Interp *UNUSED(interp), Tcl_Obj *objPtr,  Nsf_Param const *UNUSED(pPtr),
                 ClientData *UNUSED(clientData), Tcl_Obj **outObjPtr) {

  nonnull_assert(objPtr != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert(*outObjPtr == objPtr);

  *outObjPtr = objPtr;
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToTclobj --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    passed Tcl_Obj. Optionally this converter checks if the Tcl_Obj has
 *    permissible content via the Tcl "string is" checkers.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
enum stringTypeIdx {StringTypeAlnum, StringTypeAlpha, StringTypeAscii, StringTypeBoolean, StringTypeControl,
                    StringTypeDigit, StringTypeDouble, StringTypeFalse, StringTypeGraph, StringTypeInteger,
                    StringTypeLower, StringTypePrint, StringTypePunct, StringTypeSpace, StringTypeTrue,
                    StringTypeUpper, StringTypeWideinteger, StringTypeWordchar, StringTypeXdigit };
static const char *stringTypeOpts[] = {"alnum", "alpha", "ascii", "boolean", "control",
                                       "digit", "double", "false", "graph", "integer",
                                       "lower", "print", "punct", "space",  "true",
                                       "upper", "wideinteger", "wordchar", "xdigit",
                                       NULL};

int Nsf_ConvertToTclobj(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                           ClientData *clientData, Tcl_Obj **outObjPtr) nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToTclobj(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                           ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  if (unlikely(pPtr->converterArg != NULL)) {
    Tcl_Obj *objv[4];
    /*fprintf(stderr, "ConvertToTclobj %s (must be %s)\n", ObjStr(objPtr), ObjStr(pPtr->converterArg));*/

    objv[1] = pPtr->converterArg;
    objv[2] = NsfGlobalObjs[NSF_OPTION_STRICT];
    objv[3] = objPtr;

    result = NsfCallCommand(interp, NSF_STRING_IS, 4, objv);
    if (likely(result == TCL_OK)) {
      int success;
      Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &success);
      if (success == 1) {
        *clientData = objPtr;
      } else {
        Tcl_ResetResult(interp);
        result = NsfObjErrType(interp, NULL, objPtr, ObjStr(pPtr->converterArg), (Nsf_Param *)pPtr);
      }
    }
  } else {
    result = TCL_OK;
#if defined(NSF_WITH_VALUE_WARNINGS)
    if (RUNTIME_STATE(interp)->debugLevel > 0) {
      const char *value = ObjStr(objPtr);
      if (unlikely(*value == '-'
                   && (pPtr->flags & NSF_ARG_CHECK_NONPOS) != 0u
                   && isalpha(*(value+1))
                   && strchr(value+1, ' ') == NULL)
          ) {
        /*
         * In order to flag a warning, we set the error message and
         * return TCL_CONTINUE
         */
        (void)NsfPrintError(interp, "value '%s' of parameter '%s' could be a non-positional argument",
                      value, pPtr->name);
        result = TCL_CONTINUE;
      }
    }
#endif
    *clientData = objPtr;
  }
  assert(*outObjPtr == objPtr);
  return result;
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToBoolean --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    internal representation of a boolean. This converter checks the passed
 *    value via Tcl_GetBooleanFromObj().
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToBoolean(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                            ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToBoolean(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                            ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result, bool;
  result = Tcl_GetBooleanFromObj(interp, objPtr, &bool);

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  if (likely(result == TCL_OK)) {
    *clientData = (ClientData)INT2PTR(bool);
  } else {
    Tcl_ResetResult(interp);
    NsfObjErrType(interp, NULL, objPtr, "boolean", pPtr);
  }
  assert(*outObjPtr == objPtr);
  return result;
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToInt32 --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    internal representation of an integer. This converter checks the passed
 *    value via Tcl_GetIntFromObj().
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
int Nsf_ConvertToInt32(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                   ClientData *clientData, Tcl_Obj **outObjPtr) nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToInt32(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                   ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;
  int i;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  result = Tcl_GetIntFromObj(interp, objPtr, &i);

  if (likely(result == TCL_OK)) {
    *clientData = (ClientData)INT2PTR(i);
    assert(*outObjPtr == objPtr);
  } else {
    Tcl_ResetResult(interp);
    NsfObjErrType(interp, NULL, objPtr, "int32", (Nsf_Param *)pPtr);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToInteger --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    TclObj containing the bignum value. This converter checks the passed
 *    value via Tcl_GetBignumFromObj().
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

#include <tclTomMath.h>
int Nsf_ConvertToInteger(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                     ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToInteger(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
                     ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  /*
   * Try to short_cut common cases to avoid conversion to bignums, since
   * Tcl_GetBignumFromObj returns a value, which has to be freed.
   */
  if (objPtr->typePtr == Nsf_OT_intType) {
    /*
     * We know already, that the value is an int
     */
    result = TCL_OK;
  } else if (objPtr->typePtr == Nsf_OT_doubleType) {
    /*
     * We know already, that the value is not an int
     */
    result = TCL_ERROR;
  } else {
    mp_int bignumValue;

    /*
     * We have to figure out, whether the value is an int. We perform this
     * test via Tcl_GetBignumFromObj(), which tries to keep the type small if
     * possible (e.g. it might return type "int" or "float" when appropriate.
     */

    /*if (objPtr->typePtr != NULL) {
      fprintf(stderr, "type is on call %p %s value %s \n",
          objPtr->typePtr, (objPtr->typePtr != NULL) ? objPtr->typePtr->name:"NULL", ObjStr(objPtr));
          }*/

    if ((result = Tcl_GetBignumFromObj(interp, objPtr, &bignumValue)) == TCL_OK) {
      mp_clear(&bignumValue);
    }
  }

  if (likely(result == TCL_OK)) {
    *clientData = (ClientData)objPtr;
    assert(*outObjPtr == objPtr);
  } else {
    Tcl_ResetResult(interp);
    NsfObjErrType(interp, NULL, objPtr, "integer", (Nsf_Param *)pPtr);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToSwitch --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    internal representation of an Boolean. This converter simply calls
 *    Tcl_ConvertToBoolean(). The distinction between "switch" and boolean is
 *    made on the semantics of which arguments/defaults are passed to the real
 *    converter.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToSwitch(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                           ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToSwitch(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                           ClientData *clientData, Tcl_Obj **outObjPtr) {
  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  return Nsf_ConvertToBoolean(interp, objPtr, pPtr, clientData, outObjPtr);
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToObject --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    internal representation of an object. This converter checks the passed
 *    value via IsObjectOfType().
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToObject(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToObject(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                ClientData *clientData, Tcl_Obj **outObjPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert(*outObjPtr == objPtr);

  if (likely(GetObjectFromObj(interp, objPtr, (NsfObject **)clientData) == TCL_OK)) {
    return IsObjectOfType(interp, (NsfObject *)*clientData, "object", objPtr, pPtr);
  }
  return NsfObjErrType(interp, NULL, objPtr, "object", (Nsf_Param *)pPtr);
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToClass --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    internal representation of a class. This converter checks the passed
 *    value via IsObjectOfType().
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToClass(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToClass(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert(*outObjPtr == objPtr);

  if (likely(GetClassFromObj(interp, objPtr, (NsfClass **)clientData, 0) == TCL_OK)) {
    return IsObjectOfType(interp, (NsfObject *)*clientData, "class", objPtr, pPtr);
  }
  return NsfObjErrType(interp, NULL, objPtr, "class", (Nsf_Param *)pPtr);
}



/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToFilterreg --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    Tcl_Obj. This nsf type converter checks the passed value via the
 *    NsfFilterregObjType tcl_obj converter, which provides an internal
 *    representation for the client function.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToFilterreg(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToFilterreg(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  assert(*outObjPtr == objPtr);
  result = Tcl_ConvertToType(interp, objPtr, &NsfFilterregObjType);
  if (likely(result == TCL_OK)) {
    *clientData = objPtr;
    return result;
  }
  return NsfObjErrType(interp, NULL, objPtr, "filterreg", (Nsf_Param *)pPtr);
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToMixinreg --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    Tcl_Obj. This nsf type converter checks the passed value via the
 *    NsfMixinregObjType tcl_obj converter, which provides an internal
 *    representation for the client function.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToMixinreg(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToMixinreg(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
               ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert(*outObjPtr == objPtr);

  result = Tcl_ConvertToType(interp, objPtr, &NsfMixinregObjType);
  if (likely(result == TCL_OK)) {
    *clientData = objPtr;
    return result;
  }
  return NsfObjErrType(interp, NULL, objPtr, "mixinreg", (Nsf_Param *)pPtr);
}

/*
 *----------------------------------------------------------------------
 * Nsf_ConvertToParameter --
 *
 *    Nsf_TypeConverter setting the client data (passed to C functions) to the
 *    Tcl_Obj. This nsf type converter checks if the provided value could be a
 *    valid parameter spec (i.e. start with no ":", is not an unnamed spec
 *    "-:int"). This converter performs just a rough syntactic check.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int Nsf_ConvertToParameter(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                   ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

int
Nsf_ConvertToParameter(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *pPtr,
                   ClientData *clientData, Tcl_Obj **outObjPtr) {
  const char *value = ObjStr(objPtr);

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert(*outObjPtr == objPtr);

  /*fprintf(stderr, "convert to parameter '%s' t '%s'\n", value, pPtr->type);*/
  if (*value == ':' ||  (*value == '-' && *(value + 1) == ':')) {
    return NsfPrintError(interp, "leading colon in '%s' not allowed in parameter specification '%s'",
                         ObjStr(objPtr), pPtr->name);
  }

  *clientData = (char *)ObjStr(objPtr);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ConvertViaCmd --
 *
 *    Nsf_TypeConverter calling a used-defined checking/conversion
 *    function. It sets the client data (passed to C functions) to the
 *    Tcl_Obj.
 *
 * Results:
 *    Tcl result code, *clientData and **outObjPtr
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int ConvertViaCmd(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
              ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5);

static int
ConvertViaCmd(Tcl_Interp *interp, Tcl_Obj *objPtr,  Nsf_Param const *pPtr,
              ClientData *clientData, Tcl_Obj **outObjPtr) {
  Tcl_Obj *ov[5], *savedResult;
  NsfObject *object;
  int result, oc;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  /*
   * In general, when the converter is used e.g. for result checking,
   * we do not want to alter the result just when the converter sets a
   * result. So, for non-converter, we save the old result and restore
   * it before the return in case of success. Strictly speaking,
   * result-overwriting just harms for result-converters, but saving is
   * always semantically correct.
   */
  if (unlikely((pPtr->flags & NSF_ARG_IS_CONVERTER) == 0u)) {
    savedResult = Tcl_GetObjResult(interp); /* save the result */
    INCR_REF_COUNT(savedResult);
  } else {
    savedResult = NULL;
  }

  ov[0] = (pPtr->slotObj != NULL) ? pPtr->slotObj : NsfGlobalObjs[NSF_METHOD_PARAMETER_SLOT_OBJ];
  ov[1] = pPtr->converterName;
  ov[2] = pPtr->nameObj;
  ov[3] = objPtr;

  oc = 4;
  if (pPtr->converterArg != NULL) {
    ov[4] = pPtr->converterArg;
    oc++;
  }

  /*fprintf(stderr, "ConvertViaCmd call converter %s (refCount %d) on %s paramPtr %p arg %p oc %d\n",
          ObjStr(pPtr->converterName), pPtr->converterName->refCount, ObjStr(ov[0]),
          pPtr, pPtr->converterArg, oc);*/

  INCR_REF_COUNT(ov[1]);
  INCR_REF_COUNT(ov[2]);

  /* result = Tcl_EvalObjv(interp, oc, ov, 0); */
  result = GetObjectFromObj(interp, ov[0], &object);
  if(likely(result == TCL_OK)) {
    result = ObjectDispatch(object, interp, oc, ov, NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS);
  }

  DECR_REF_COUNT(ov[1]);
  DECR_REF_COUNT(ov[2]);

  /* per default, the input arg is the output arg */
  assert(*outObjPtr == objPtr);

  if (likely(result == TCL_OK)) {
    /*fprintf(stderr, "ConvertViaCmd could convert %s to '%s' paramPtr %p, is_converter %d\n",
            ObjStr(objPtr), ObjStr(Tcl_GetObjResult(interp)), pPtr,
            pPtr->flags & NSF_ARG_IS_CONVERTER);*/
    if ((pPtr->flags & NSF_ARG_IS_CONVERTER) != 0u) {
      Tcl_Obj *resultObj;
      /*
       * If we want to convert, the resulting obj is the result of the
       * converter. incr refCount is necessary e.g. for e.g.
       *
       *     return [expr {$value + 1}]
       *
       * The conversion is just needed, when resultObj differs from the actual
       * value in the output vector. Otherwise the conversion and the value
       * increment happened already before (and is already recorded in the parse context).
       */
      resultObj = Tcl_GetObjResult(interp);

      if (*outObjPtr != resultObj) {
        INCR_REF_COUNT2("valueObj", resultObj);
        *outObjPtr = resultObj;
      }
      /*fprintf(stderr, "**** NSF_ARG_IS_CONVERTER %p\n", *outObjPtr);*/
    }
    *clientData = (ClientData) *outObjPtr;

    if (savedResult != NULL) {
      /*fprintf(stderr, "restore savedResult %p\n", savedResult);*/
      Tcl_SetObjResult(interp, savedResult);  /* restore the result */
    }
  }

  if (savedResult != NULL) {
    DECR_REF_COUNT(savedResult);
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * ConvertToObjpattern --
 *
 *    This function obtains a Tcl_Obj *, which contains the pattern if an Next
 *    Scripting Object. When this pattern contains no meta characters, we
 *    check if the object exists. If it exists, the Tcl_Obj is converted to
 *    the cmd-type. If it does not exit, the function using this pattern will
 *    fail. If the pattern contains meta characters, we prepend to the pattern
 *    "::" if necessary to avoid errors, if one specifies a pattern object
 *    without the prefix. In this case, the patternObj is is of plain type.
 *    The resulting patternObj has always the refCount incremented, which has
 *    to be decremented by the caller.x
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Incremented refCount for the patternObj.
 *
 *----------------------------------------------------------------------
 */
static int ConvertToObjpattern(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *UNUSED(pPtr),
                    ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);

static int
ConvertToObjpattern(Tcl_Interp *interp, Tcl_Obj *objPtr, Nsf_Param const *UNUSED(pPtr),
                    ClientData *clientData, Tcl_Obj **outObjPtr) {
  Tcl_Obj *patternObj = objPtr;
  const char *pattern = ObjStr(objPtr);

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  if (NoMetaChars(pattern)) {
    /*
     * We have no meta characters, we try to check for an existing object
     */
    NsfObject *object = NULL;
    if (GetObjectFromObj(interp, objPtr, &object) == TCL_OK && object != NULL) {
      patternObj = object->cmdName;
    }
  } else {
    /*
     * We have a pattern and meta characters, we might have
     * to prefix it to ovoid obvious errors: since all object
     * names are prefixed with ::, we add this prefix automatically
     * to the match pattern, if it does not exist.
     */
    if (*pattern != ':' && *pattern+1 != ':') {
      patternObj = Tcl_NewStringObj("::", 2);
      Tcl_AppendLimitedToObj(patternObj, pattern, -1, INT_MAX, NULL);
    }
  }
  if (patternObj != NULL) {
    INCR_REF_COUNT2("patternObj", patternObj);
  }
  *clientData = (ClientData)patternObj;
  /* The following assert does not hold here, since we
     have a direct call to the converter
     assert(*outObjPtr == objPtr); */

  *outObjPtr = objPtr;
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ParamCheckObj --
 *
 *    Ths function returns a fresh Tcl_Obj in the form of a method name for a
 *    checker method.
 *
 * Results:
 *    Tcl_Obj
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *ParamCheckObj(const char *start, size_t len) nonnull(1) returns_nonnull;

static Tcl_Obj *
ParamCheckObj(const char *start, size_t len) {
  Tcl_Obj *checker = Tcl_NewStringObj("type=", 5);

  nonnull_assert(start != NULL);

  Tcl_AppendLimitedToObj(checker, start, len, INT_MAX, NULL);
  return checker;
}

/*
 *----------------------------------------------------------------------
 * ParamOptionSetConverter --
 *
 *    Fill in the fields int to the specifed paramPtr structure
 *    checker method and perform sanity checking.
 *
 * Results:
 *    Standard result code
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int ParamOptionSetConverter(Tcl_Interp *interp, Nsf_Param *paramPtr,
                        const char *typeName, Nsf_TypeConverter *converter)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
ParamOptionSetConverter(Tcl_Interp *interp, Nsf_Param *paramPtr,
                        const char *typeName, Nsf_TypeConverter *converter) {

  nonnull_assert(interp != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(typeName != NULL);
  nonnull_assert(converter != NULL);

  if (paramPtr->converter != NULL) {
    return NsfPrintError(interp, "refuse to redefine parameter type of '%s' from type '%s' to type '%s'",
                         paramPtr->name, paramPtr->type, typeName);
  }
  paramPtr->converter = converter;
  paramPtr->nrArgs = 1;
  paramPtr->type = typeName;
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * Unescape --
 *
 *    Unescape double commas in the provided Tcl_Obj.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Potentially shortend string content
 *
 *----------------------------------------------------------------------
 */

static void Unescape(Tcl_Obj *objPtr) nonnull(1);

static void
Unescape(Tcl_Obj *objPtr) {
  int i, j, l;
  char *string;

  nonnull_assert(objPtr != NULL);

  l = Tcl_GetCharLength(objPtr);
  string = ObjStr(objPtr);

  for (i = 0; i < l; i++) {
    if (string[i] == ',' && string[i+1] == ',') {
      for (j = i+1; j < l; j++) {
        string[j] = string[j+1];
      }
      l--;
      i++;
    }
  }
  Tcl_SetObjLength(objPtr, l);
}

/*
 *----------------------------------------------------------------------
 * ParamOptionParse --
 *
 *    Parse a single parameter option of a parameter. The parameter option
 *    string is passed in as second argument, the sizes start and remainder
 *    flag the offsets in the string. As a result, the fields of the parameter
 *    structure are updated.
 *
 * Results:
 *    Tcl result code, updated fields in the Nsf_Param structure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int ParamOptionParse(Tcl_Interp *interp, const char *argString,
                 size_t start, size_t optionLength,
                 int disallowedOptions, Nsf_Param *paramPtr, int unescape)
  nonnull(1) nonnull(2) nonnull(6);

static int
ParamOptionParse(Tcl_Interp *interp, const char *argString,
                 size_t start, size_t optionLength,
                 int disallowedOptions, Nsf_Param *paramPtr, int unescape) {
  const char *dotdot, *option = argString + start;
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(argString != NULL);
  nonnull_assert(paramPtr != NULL);

  /*fprintf(stderr, "ParamOptionParse name %s, option '%s' (%ld) disallowed %.6x\n",
    paramPtr->name, option, start, disallowedOptions);*/

  if (strncmp(option, "required", MAX(3, optionLength)) == 0) {
    paramPtr->flags |= NSF_ARG_REQUIRED;

  } else if (strncmp(option, "optional",  MAX(3, optionLength)) == 0) {
    paramPtr->flags &= ~NSF_ARG_REQUIRED;

  } else if (strncmp(option, "substdefault", 12) == 0) {
    paramPtr->flags |= NSF_ARG_SUBST_DEFAULT;

  } else if (strncmp(option, "convert", 7) == 0) {
    paramPtr->flags |= NSF_ARG_IS_CONVERTER;

  } else if (strncmp(option, "initcmd", 7) == 0) {
    if (unlikely((paramPtr->flags & (NSF_ARG_CMD|NSF_ARG_ALIAS|NSF_ARG_FORWARD)) != 0u)) {
      return NsfPrintError(interp, "parameter option 'initcmd' not valid in this option combination");
    }
    paramPtr->flags |= NSF_ARG_INITCMD;

  } else if (strncmp(option, "cmd", 3) == 0) {
    if (unlikely((paramPtr->flags & (NSF_ARG_INITCMD|NSF_ARG_ALIAS|NSF_ARG_FORWARD)) != 0u)) {
      return NsfPrintError(interp, "parameter option 'cmd' not valid in this option combination");
    }
    paramPtr->flags |= NSF_ARG_CMD;

  } else if (strncmp(option, "alias", 5) == 0) {
    if (unlikely((paramPtr->flags & (NSF_ARG_INITCMD|NSF_ARG_CMD|NSF_ARG_FORWARD)) != 0u)) {
      return NsfPrintError(interp, "parameter option 'alias' not valid in this option combination");
    }
    paramPtr->flags |= NSF_ARG_ALIAS;

  } else if (strncmp(option, "forward", 7) == 0) {
    if (unlikely((paramPtr->flags & (NSF_ARG_INITCMD|NSF_ARG_CMD|NSF_ARG_ALIAS)) != 0u)) {
      return NsfPrintError(interp, "parameter option 'forward' not valid in this option combination");
    }
    paramPtr->flags |= NSF_ARG_FORWARD;

  } else if (strncmp(option, "slotset", 7) == 0) {
    if (unlikely(paramPtr->slotObj == NULL)) {
      return NsfPrintError(interp, "parameter option 'slotset' must follow 'slot='");
    }
    paramPtr->flags |= NSF_ARG_SLOTSET;

  } else if (strncmp(option, "slotinitialize", 14) == 0) {
    if (unlikely(paramPtr->slotObj == NULL)) {
      return NsfPrintError(interp, "parameter option 'slotinit' must follow 'slot='");
    }
    paramPtr->flags |= NSF_ARG_SLOTINITIALIZE;

  } else if ((dotdot = strnstr(option, "..", optionLength-1))) {
    /* check lower bound */
    if (*option == '0') {
      paramPtr->flags |= NSF_ARG_ALLOW_EMPTY;
    } else if (unlikely(*option != '1')) {
      return NsfPrintError(interp, "lower bound of multiplicity in %s not supported", argString);
    }
    /* check upper bound */
    option = dotdot + 2;
    if (*option == '*' || *option == 'n') {
      if (unlikely((paramPtr->flags & (NSF_ARG_SWITCH)) != 0u)) {
        return NsfPrintError(interp,
                             "upper bound of multiplicity of '%c' not allowed for \"switch\"\n", *option);
      }
      paramPtr->flags |= NSF_ARG_MULTIVALUED;
    } else if (*option != '1') {
      return NsfPrintError(interp, "upper bound of multiplicity in %s not supported", argString);
    }

  } else if (strncmp(option, "noarg", 5) == 0) {
    if ((paramPtr->flags & NSF_ARG_ALIAS) == 0u) {
      return NsfPrintError(interp, "parameter option \"noarg\" only allowed for parameter type \"alias\"");
    }
    paramPtr->flags |= NSF_ARG_NOARG;
    paramPtr->nrArgs = 0;

  } else if (strncmp(option, "nodashalnum", 11) == 0) {
    if (*paramPtr->name == '-') {
      return NsfPrintError(interp, "parameter option 'nodashalnum' only allowed for positional parameters");
    }
    paramPtr->flags |= NSF_ARG_NODASHALNUM;

  } else if (strncmp(option, "noconfig", 8) == 0) {
    if (disallowedOptions != NSF_DISALLOWED_ARG_OBJECT_PARAMETER) {
      return NsfPrintError(interp, "parameter option 'noconfig' only allowed for object parameters");
    }
    paramPtr->flags |= NSF_ARG_NOCONFIG;

  } else if (strncmp(option, "args", 4) == 0) {
    if ((paramPtr->flags & NSF_ARG_ALIAS) == 0u) {
      return NsfPrintError(interp, "parameter option \"args\" only allowed for parameter type \"alias\"");
    }
    result = ParamOptionSetConverter(interp, paramPtr, "args", ConvertToNothing);

  } else if (optionLength >= 4 && strncmp(option, "arg=", 4) == 0) {
    if (paramPtr->converter != ConvertViaCmd) {
      return NsfPrintError(interp,
                           "parameter option 'arg=' only allowed for user-defined converter");
    }
    if (paramPtr->converterArg != NULL) {DECR_REF_COUNT(paramPtr->converterArg);}
    paramPtr->converterArg = Tcl_NewStringObj(option + 4, optionLength - 4);
    /*
     * In case, we know that we have to unescape double commas, do it here...
     */
    if (unlikely(unescape)) {
      Unescape(paramPtr->converterArg);
    }
    INCR_REF_COUNT(paramPtr->converterArg);

  } else if (strncmp(option, "switch", 6) == 0) {
    if (*paramPtr->name != '-') {
      return NsfPrintError(interp,
                          "invalid parameter type \"switch\" for argument \"%s\"; "
                           "type \"switch\" only allowed for non-positional arguments",
                           paramPtr->name);
    } else if ((paramPtr->flags & NSF_ARG_METHOD_INVOCATION) != 0u) {
      return NsfPrintError(interp, "parameter invocation types cannot be used with option 'switch'");
    }
    result = ParamOptionSetConverter(interp, paramPtr, "switch", Nsf_ConvertToSwitch);
    paramPtr->flags |= NSF_ARG_SWITCH;
    paramPtr->nrArgs = 0;
    assert(paramPtr->defaultValue == NULL);
    paramPtr->defaultValue = Tcl_NewBooleanObj(0);
    INCR_REF_COUNT(paramPtr->defaultValue);

  } else if (strncmp(option, "integer", MAX(3, optionLength)) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "integer", Nsf_ConvertToInteger);

  } else if (strncmp(option, "int32", 5) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "int32", Nsf_ConvertToInt32);

  } else if (strncmp(option, "boolean", 7) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "boolean", Nsf_ConvertToBoolean);

  } else if (strncmp(option, "object", 6) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "object", Nsf_ConvertToObject);

  } else if (strncmp(option, "class", 5) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "class", Nsf_ConvertToClass);

  } else if (strncmp(option, "metaclass", 9) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "class", Nsf_ConvertToClass);
    paramPtr->flags |= NSF_ARG_METACLASS;

  } else if (strncmp(option, "baseclass", 9) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "class", Nsf_ConvertToClass);
    paramPtr->flags |= NSF_ARG_BASECLASS;

  } else if (strncmp(option, "mixinreg", 8) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "mixinreg", Nsf_ConvertToMixinreg);

 } else if (strncmp(option, "filterreg", 9) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "filterreg", Nsf_ConvertToFilterreg);

  } else if (strncmp(option, "parameter", 9) == 0) {
    result = ParamOptionSetConverter(interp, paramPtr, "parameter", Nsf_ConvertToParameter);

  } else if (optionLength >= 6 && strncmp(option, "type=", 5) == 0) {
    if (paramPtr->converter != Nsf_ConvertToObject &&
        paramPtr->converter != Nsf_ConvertToClass)
        return NsfPrintError(interp, "parameter option 'type=' only allowed for parameter types 'object' and 'class'");
    if (paramPtr->converterArg != NULL) {DECR_REF_COUNT(paramPtr->converterArg);}
    paramPtr->converterArg = Tcl_NewStringObj(option + 5, optionLength - 5);
    if (unlikely(unescape)) {
      Unescape(paramPtr->converterArg);
    }
    INCR_REF_COUNT(paramPtr->converterArg);

  } else if (optionLength >= 6 && strncmp(option, "slot=", 5) == 0) {
    if (paramPtr->slotObj != NULL) {DECR_REF_COUNT(paramPtr->slotObj);}
    paramPtr->slotObj = Tcl_NewStringObj(option + 5, optionLength - 5);
    if (unlikely(unescape)) {
      Unescape(paramPtr->slotObj);
    }
    INCR_REF_COUNT(paramPtr->slotObj);

  } else if (optionLength >= 6 && strncmp(option, "method=", 7) == 0) {
    if ((paramPtr->flags & (NSF_ARG_ALIAS|NSF_ARG_FORWARD|NSF_ARG_SLOTSET)) == 0u) {
      return NsfPrintError(interp, "parameter option 'method=' only allowed for parameter "
                           "types 'alias', 'forward' and 'slotset'");
    }
    if (paramPtr->method != NULL) {DECR_REF_COUNT(paramPtr->method);}
    paramPtr->method = Tcl_NewStringObj(option + 7, optionLength - 7);
    if (unlikely(unescape)) {
      Unescape(paramPtr->method);
    }
    INCR_REF_COUNT(paramPtr->method);

  } else {
    Tcl_DString ds, *dsPtr = &ds;

    if (option[0] == '\0') {
      NsfLog(interp, NSF_LOG_WARN, "empty parameter option ignored");
      return TCL_OK;
    }

    Tcl_DStringInit(dsPtr);
    Tcl_DStringAppend(dsPtr, option, optionLength);

    if (unlikely(paramPtr->converter != NULL)) {
      NsfPrintError(interp, "parameter option '%s' unknown for parameter type '%s'",
                    Tcl_DStringValue(dsPtr), paramPtr->type);
      Tcl_DStringFree(dsPtr);
      return TCL_ERROR;
    }

    if (Nsf_PointerTypeLookup(interp, Tcl_DStringValue(dsPtr))) {
      /*
       * Check, if the option refers to a pointer converter
       */
      ParamOptionSetConverter(interp, paramPtr,  Tcl_DStringValue(dsPtr), Nsf_ConvertToPointer);
      Tcl_DStringFree(dsPtr);

    } else {
      int i, found = -1;

      /*
       * The option is still unknown, check the Tcl string-is checkers
       */
      Tcl_DStringFree(dsPtr);

      for (i = 0; stringTypeOpts[i]; i++) {
        /*
         * Do not allow abbreviations, so the additional strlen checks
         * for a full match
         */
        if (strncmp(option, stringTypeOpts[i], optionLength) == 0
            && strlen(stringTypeOpts[i]) == optionLength) {
          found = i;
          break;
        }
      }

      if (found > -1) {
        /* converter is stringType */
        result = ParamOptionSetConverter(interp, paramPtr, "stringtype", Nsf_ConvertToTclobj);
        if (paramPtr->converterArg != NULL) {DECR_REF_COUNT(paramPtr->converterArg);}
        paramPtr->converterArg = Tcl_NewStringObj(stringTypeOpts[i], -1);
        INCR_REF_COUNT(paramPtr->converterArg);
      } else {

        /*
         * The parameter option is still unknown. We assume that the parameter
         * option identifies a user-defined argument checker, implemented as a
         * method.
         */
        if (paramPtr->converterName != NULL) {
          DECR_REF_COUNT2("converterNameObj", paramPtr->converterName);
        }
        paramPtr->converterName = ParamCheckObj(option, optionLength);
        INCR_REF_COUNT2("converterNameObj", paramPtr->converterName);
        result = ParamOptionSetConverter(interp, paramPtr, ObjStr(paramPtr->converterName), ConvertViaCmd);
      }
    }
  }

  if ((paramPtr->flags & disallowedOptions) != 0u) {
    return NsfPrintError(interp, "parameter option '%s' not allowed", option);
  }

  if (unlikely(((paramPtr->flags & NSF_ARG_METHOD_INVOCATION) != 0u)
               && ((paramPtr->flags & NSF_ARG_NOCONFIG)) != 0u)) {
    return NsfPrintError(interp, "parameter option 'noconfig' cannot used together with this type of object parameter");
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * ParamParse --
 *
 *    Parse a a single parameter with a possible default provided in the form
 *    of an Tcl_Obj.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int ParamParse(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Obj *arg, unsigned int disallowedFlags,
           Nsf_Param *paramPtr, int *possibleUnknowns, int *plainParams, int *nrNonposArgs)
  nonnull(1) nonnull(3) nonnull(5) nonnull(6) nonnull(7) nonnull(8);

static int
ParamParse(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Obj *arg, unsigned int disallowedFlags,
           Nsf_Param *paramPtr, int *possibleUnknowns, int *plainParams, int *nrNonposArgs) {
  int result, npac, isNonposArgument, parensCount;
  size_t length, j;
  const char *argString, *argName;
  Tcl_Obj **npav;

  nonnull_assert(interp != NULL);
  nonnull_assert(arg != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(possibleUnknowns != NULL);
  nonnull_assert(plainParams != NULL);
  nonnull_assert(nrNonposArgs != NULL);

  paramPtr->paramObj = arg;
  INCR_REF_COUNT(paramPtr->paramObj);

  result = Tcl_ListObjGetElements(interp, arg, &npac, &npav);
  if (unlikely(result != TCL_OK || npac < 1 || npac > 2)) {
    DECR_REF_COUNT(paramPtr->paramObj);
    return NsfPrintError(interp, "wrong # of elements in parameter definition for method '%s'"
                         " (should be 1 or 2 list elements): %s",
                         ObjStr(procNameObj), ObjStr(arg));
  }

  argString = ObjStr(npav[0]);
  length = strlen(argString);

  /*
   * Per default parameter have exactly one argument; types without arguments
   * (like "switch") have to set their nrArgs explicitly.
   */
  paramPtr->nrArgs = 1;

  isNonposArgument = *argString == '-';
  if (isNonposArgument != 0) {
    argName = argString+1;
    (*nrNonposArgs) ++;
  } else {
    argName = argString;
    paramPtr->flags |= NSF_ARG_REQUIRED; /* positional arguments are required unless we have a default */
  }

  /*fprintf(stderr, "... parsing '%s', name '%s' argString '%s' \n",
    ObjStr(arg), argName, argString);*/

  /*
   * Find the first ':' outside of parens; the name of the parameter might be
   * in array syntax, the array index might contain ":", "," etc.
   */
  parensCount = 0;
  for (j = 0; j < length; j++) {
    if (parensCount > 0 && argString[j] == ')') {
      parensCount --;
      continue;
    }
    if (argString[j] == '(') {
      parensCount ++;
      continue;
    }
    if (parensCount == 0 && argString[j] == ':') {
      break;
    }
  }

  if (argString[j] == ':') {
    /* we found a ':' */
    size_t l, start, end;
    int unescape = 0;

    /* get parameter name */
    STRING_NEW(paramPtr->name, argString, j);
    paramPtr->nameObj = Tcl_NewStringObj(argName, (isNonposArgument != 0) ? j-1 : j);
    INCR_REF_COUNT(paramPtr->nameObj);

    /* skip space at begin */
    for (start = j+1; start<length && isspace((int)argString[start]); start++) {;}

    /* search for unescaped ',' */
    for (l = start; l < length; l++) {
      if (unlikely(argString[l] == ',')) {
        if (likely(argString[l+1]) == ',') {
          l++;
          unescape = 1;
          continue;
        }
        /* skip space from end */
        for (end = l; end > 0 && isspace((int)argString[end-1]); end--);
        result = ParamOptionParse(interp, argString, start, end-start, disallowedFlags, paramPtr, unescape);
        unescape = 0;
        if (unlikely(result != TCL_OK)) {
          goto param_error;
        }
        l++;
        /* skip space from begin */
        for (start = l; start<length && isspace((int)argString[start]); start++) {;}
      }
    }
    /* skip space from end */
    for (end = l; end > 0 && isspace((int)argString[end-1]); end--);
    /* process last option */
    if (end-start > 0) {
      result = ParamOptionParse(interp, argString, start, end-start, disallowedFlags, paramPtr, unescape);
      if (unlikely(result != TCL_OK)) {
        goto param_error;
      }
    }

  } else {
    /* no ':', the whole arg is the name, we have no options */
    STRING_NEW(paramPtr->name, argString, length);
    if (isNonposArgument != 0) {
      paramPtr->nameObj = Tcl_NewStringObj(argName, length-1);
    } else {
      (*plainParams) ++;
      paramPtr->nameObj = Tcl_NewStringObj(argName, length);
    }
    INCR_REF_COUNT(paramPtr->nameObj);
  }

  /* if we have two arguments in the list, the second one is a default value */
  if (npac == 2) {

    if ((disallowedFlags & NSF_ARG_HAS_DEFAULT) != 0u) {
      NsfPrintError(interp, "parameter \"%s\" is not allowed to have default \"%s\"",
                    argString, ObjStr(npav[1]));
      goto param_error;
    }

    /* if we have for some reason already a default value, free it */
    if (paramPtr->defaultValue != NULL) {
      DECR_REF_COUNT(paramPtr->defaultValue);
    }
    paramPtr->defaultValue = Tcl_DuplicateObj(npav[1]);
    INCR_REF_COUNT(paramPtr->defaultValue);
    /*
     * The argument will be not required for an invocation, since we
     * have a default.
     */
    paramPtr->flags &= ~NSF_ARG_REQUIRED;
  } else if ((paramPtr->flags & NSF_ARG_SUBST_DEFAULT) != 0u) {
    NsfPrintError(interp,
                  "parameter option substdefault specified for parameter \"%s\""
                  " without default value", paramPtr->name);
    goto param_error;
  }

  /* postprocessing the parameter options */

  if (paramPtr->converter == NULL) {
    /*
     * If no converter is set, use the default converter
     */
    paramPtr->converter = Nsf_ConvertToTclobj;
  } else if (paramPtr->converter == ConvertToNothing) {
    if ((paramPtr->flags & (NSF_ARG_ALLOW_EMPTY|NSF_ARG_MULTIVALUED)) != 0u) {
      NsfPrintError(interp,
                    "multiplicity settings for variable argument parameter \"%s\" not allowed",
                    paramPtr->name);
      goto param_error;
    }
  }

  /*
   * Check for application specific value checkers and converters
   */
  /*fprintf(stderr, "parm %s: slotObj %p viaCmd? %d\n",
          paramPtr->name, paramPtr->slotObj, paramPtr->converter == ConvertViaCmd);*/

  if ((paramPtr->slotObj || paramPtr->converter == ConvertViaCmd) && paramPtr->type) {
    const char *converterNameString;
    Tcl_Obj *converterNameObj, *slotObj;
    NsfObject *paramObject;
    Tcl_Command cmd;
    NsfClass *pcl = NULL;

    slotObj = (paramPtr->slotObj != NULL) ? paramPtr->slotObj : NsfGlobalObjs[NSF_METHOD_PARAMETER_SLOT_OBJ];
    result = GetObjectFromObj(interp, slotObj, &paramObject);
    if (unlikely(result != TCL_OK)) {
      NsfPrintError(interp, "non-existing slot object \"%s\"", ObjStr(slotObj));
      goto param_error;
    }
    if (paramPtr->converterName == NULL) {
      converterNameObj = ParamCheckObj(paramPtr->type, strlen(paramPtr->type));
      INCR_REF_COUNT2("converterNameObj", converterNameObj);
    } else {
      converterNameObj = paramPtr->converterName;
    }
    converterNameString = ObjStr(converterNameObj);

    cmd = ObjectFindMethod(interp, paramObject, converterNameObj, &pcl);
    /*fprintf(stderr, "locating %s on %s returns %p (%s)\n",
      ObjStr(converterNameObj), ObjectName(paramObject), cmd, ClassName(pcl));*/
    if (cmd == NULL) {
      if (paramPtr->converter == ConvertViaCmd) {

        NsfLog(interp, NSF_LOG_WARN, "Could not find value checker %s defined on %s",
               converterNameString, ObjectName(paramObject));

        paramPtr->flags |= NSF_ARG_CURRENTLY_UNKNOWN;
        /* TODO: for the time being, we do not return an error here */
      }
    } else if (paramPtr->converter != ConvertViaCmd &&
               paramPtr->slotObj &&
               strcmp(ObjStr(paramPtr->slotObj),
                      NsfGlobalStrings[NSF_METHOD_PARAMETER_SLOT_OBJ]) != 0) {

      NsfLog(interp, NSF_LOG_WARN, "Checker method %s defined on %s shadows built-in converter",
             converterNameString, ObjectName(paramObject));

      if (paramPtr->converterName == NULL) {
        paramPtr->converterName = converterNameObj;
        paramPtr->converter = NULL;
        result = ParamOptionSetConverter(interp, paramPtr, converterNameString, ConvertViaCmd);
        if (unlikely(result != TCL_OK)) {
          if (converterNameObj != paramPtr->converterName) {
            DECR_REF_COUNT2("converterNameObj", converterNameObj);
          }
          goto param_error;
        }
      }
    }
    if (((paramPtr->flags & NSF_ARG_IS_CONVERTER) != 0u)
        && paramPtr->converter != ConvertViaCmd) {
      NsfPrintError(interp, "option 'convert' only allowed for application-defined converters");
      if (converterNameObj != paramPtr->converterName) {
        DECR_REF_COUNT2("converterNameObj", converterNameObj);
      }
      goto param_error;
    }

    if (converterNameObj != paramPtr->converterName) {
      DECR_REF_COUNT2("converterNameObj", converterNameObj);
    }
  }

  /*
   * If the argument has no arguments and it is positional, it can't be
   * required.
   */
  if (paramPtr->nrArgs == 0
      && *paramPtr->name != '-'
      && (paramPtr->flags & NSF_ARG_REQUIRED) != 0u
      ) {
    paramPtr->flags &= ~NSF_ARG_REQUIRED;
  }

  /*
   * If the argument is not required and no default value is
   * specified, we have to handle in the client code (eg. in the
   * canonical arg handlers for scripted methods) the unknown value
   * (e.g. don't set/unset a variable)
   */
  if ((paramPtr->flags & NSF_ARG_REQUIRED) == 0u && paramPtr->defaultValue == NULL) {
    (*possibleUnknowns)++;
  }
  return TCL_OK;

 param_error:
  ParamFree(paramPtr);
  paramPtr->name = NULL;

#if !defined(NDEBUG)
  /*
   * Whenever we return a TCL_ERROR, we expect that the interp result contains
   * an error message.
   */
  {
    const char *errStr = ObjStr(Tcl_GetObjResult(interp));
    assert(*errStr != '\0');
  }
#endif

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 * ParamDefsParse --
 *
 *    Parse a list of parameters in the form of Tcl_Objs into a parsedParamPtr
 *    structure (last argument). The argument allowedOptions is used to flag,
 *    what parameter options are generally allowed (typically different for
 *    method and object parameters). Unless forceParamdefs is set, the parsed
 *    parameter structure is only returned when needed (i.e. when not all
 *    parameters are plain parameters).
 *
 * Results:
 *    Tcl result code, parsedParameter structure in last argument (allocated
 *    by the caller).
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int ParamDefsParse(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Obj *paramSpecObjs,
                          int allowedOptinons, int forceParamdefs, NsfParsedParam *parsedParamPtr)
  nonnull(1) nonnull(3) nonnull(6);

static int
ParamDefsParse(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Obj *paramSpecObjs,
               int allowedOptinons, int forceParamdefs, NsfParsedParam *parsedParamPtr) {
  Tcl_Obj **argsv;
  int result, argsc;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramSpecObjs != NULL);
  nonnull_assert(parsedParamPtr != NULL);

  parsedParamPtr->paramDefs = NULL;
  parsedParamPtr->possibleUnknowns = 0;

  result = Tcl_ListObjGetElements(interp, paramSpecObjs, &argsc, &argsv);
  if (unlikely(result != TCL_OK)) {
    return NsfPrintError(interp, "cannot break down non-positional args: %s", ObjStr(paramSpecObjs));
  }

  if (argsc > 0) {
    Nsf_Param *paramsPtr, *paramPtr, *lastParamPtr;
    int i, possibleUnknowns = 0, plainParams = 0, nrNonposArgs = 0;
    NsfParamDefs *paramDefs;

    paramPtr = paramsPtr = ParamsNew(argsc);

    for (i = 0; i < argsc; i++, paramPtr++) {
      result = ParamParse(interp, procNameObj, argsv[i], allowedOptinons,
                          paramPtr, &possibleUnknowns, &plainParams, &nrNonposArgs);

      if (result == TCL_OK && paramPtr->converter == ConvertToNothing && i < argsc-1) {
        result = NsfPrintError(interp,
                               "parameter option \"args\" invalid for parameter \"%s\"; only allowed for last parameter",
                               paramPtr->name);
      }
      if (unlikely(result != TCL_OK)) {
        ParamsFree(paramsPtr);
        return result;
      }
      /* every parameter must have at least a name set */
      assert(paramPtr->name);
    }
#if defined(NSF_WITH_VALUE_WARNINGS)
    if (nrNonposArgs > 0 && argsc > 1) {
      for (i = 0; i < argsc; i++) {
        (paramsPtr + i)->flags |= NSF_ARG_CHECK_NONPOS;
      }
    }
#endif

    /*
     * If all arguments are good old Tcl arguments, there is no need
     * to use the parameter definition structure.
     */
    if (plainParams == argsc && !forceParamdefs) {
      ParamsFree(paramsPtr);
      return TCL_OK;
    }
    /*
    fprintf(stderr, "we need param definition structure for {%s}, argsc %d plain %d\n",
            ObjStr(args), argsc, plainParams);
    */
    /*
     * Check the last argument. If the last argument is named 'args',
     * force converter and make it non-required.
     */
    lastParamPtr = paramPtr - 1;
    if (isArgsString(lastParamPtr->name)) {
      lastParamPtr->converter = ConvertToNothing;
      lastParamPtr->flags &= ~NSF_ARG_REQUIRED;
    }

    paramDefs = ParamDefsNew();
    paramDefs->paramsPtr = paramsPtr;
    paramDefs->nrParams = paramPtr-paramsPtr;
    /*fprintf(stderr, "method %s serial %d paramDefs %p ifsize %ld, possible unknowns = %d,\n",
            ObjStr(procNameObj), paramDefs->serial,
            paramDefs, paramPtr-paramsPtr, possibleUnknowns);*/
    parsedParamPtr->paramDefs = paramDefs;
    parsedParamPtr->possibleUnknowns = possibleUnknowns;
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ParameterMethodForwardDispatch --
 *
 *    Dispatch a forwarding method provided via parameter definition.
 *
 *    The current implementation performs for every object
 *    parameter forward the full cycle of
 *
 *     (a) splitting the spec,
 *     (b) convert it to a the client data structure,
 *     (c) invoke forward,
 *     (d) free client data structure
 *
 *    In the future, it should convert to the client data
 *    structure just once and free it with the disposal of the
 *    parameter. This could be achieved
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    The called function might side-effect.
 *
 *----------------------------------------------------------------------
 */
static int ParameterMethodForwardDispatch(Tcl_Interp *interp, NsfObject *object,
                               Nsf_Param const *paramPtr, Tcl_Obj *newValue,
                               NsfCallStackContent *cscPtr)
  nonnull(1) nonnull(2) nonnull(3);

static int
ParameterMethodForwardDispatch(Tcl_Interp *interp, NsfObject *object,
                               Nsf_Param const *paramPtr, Tcl_Obj *newValue,
                               NsfCallStackContent *cscPtr) {
  Tcl_Obj **nobjv, *ov[3], *methodObj, *forwardSpec;
  ForwardCmdClientData *tcd = NULL;
  int result, oc, nobjc;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(paramPtr != NULL);
  assert((paramPtr->flags & NSF_ARG_FORWARD) != 0u);

  forwardSpec = (paramPtr->method != NULL) ? paramPtr->method : NULL; /* different default? */
  if (forwardSpec == NULL) {
    return NsfPrintError(interp, "forward: no spec available\n");
  }

  result = Tcl_ListObjGetElements(interp, forwardSpec, &nobjc, &nobjv);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  methodObj = paramPtr->nameObj;
  result = ForwardProcessOptions(interp, methodObj,
                                 NULL /*withDefault*/,
                                 0 /*withEarlybinding*/,
                                 NULL /*withOnerror*/,
                                 NULL /*withMethodprefix*/,
                                 0 /*withFrame*/,
                                 0 /*withVerbose*/,
                                 nobjv[0], nobjc-1, nobjv+1, &tcd);
  if (unlikely(result != TCL_OK)) {
    if (tcd != NULL) {
      ForwardCmdDeleteProc(tcd);
    }
    return result;
  }

  /*fprintf(stderr, "parameter %s forward spec <%s> After Options obj %s method %s\n",
          ObjStr(paramPtr->nameObj), ObjStr(forwardSpec),
          ObjectName(object), ObjStr(methodObj));*/

  tcd->object = object;
  oc = 1;
  ov[0] = methodObj;
  if (paramPtr->nrArgs == 1 && newValue) {
    ov[oc] = newValue;
    oc ++;
  }

  /*
   * Mark the intermittent CSC frame as INACTIVE, so that, e.g.,
   * call-stack traversals seeking active frames ignore it.
   */
  if (cscPtr != NULL) {
    cscPtr->frameType = NSF_CSC_TYPE_INACTIVE;
  }

  result = NsfForwardMethod(tcd, interp, oc, ov);
  ForwardCmdDeleteProc(tcd);

  return result;
}


/*
 *----------------------------------------------------------------------
 * ParameterMethodDispatch --
 *
 *    Dispatch a method provided via parameter definition. The function checks
 *    the parameter definition, builds a argument list for the function call
 *    and invokes finally the configured cmd.  This function is typically
 *    called from configure.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    The called function might side-effect.
 *
 *----------------------------------------------------------------------
 */
static int ParameterMethodDispatch(Tcl_Interp *interp, NsfObject *object,
                                   Nsf_Param *paramPtr, Tcl_Obj *newValue,
                                   CallFrame *uplevelVarFramePtr,
                                   const char *initString,
                                   Tcl_Obj **nextObjPtr,
                                   int nrRemainingArgs)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(6) nonnull(7);

static int
ParameterMethodDispatch(Tcl_Interp *interp, NsfObject *object,
                        Nsf_Param *paramPtr, Tcl_Obj *newValue,
                        CallFrame *uplevelVarFramePtr,
                        const char *initString,
                        Tcl_Obj **nextObjPtr,
                        int nrRemainingArgs) {
  CallFrame *varFramePtr = Tcl_Interp_varFramePtr(interp);
  NsfCallStackContent csc, *cscPtr = &csc;
  CallFrame frame2, *framePtr2 = &frame2;
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(newValue != NULL);
  nonnull_assert(initString != NULL);
  nonnull_assert(nextObjPtr != NULL);

#if 0
  {int i;
  fprintf(stderr, "ParameterMethodDispatch %s flags %06x nrRemainingArgs %d ",
          paramPtr->name, paramPtr->flags, nrRemainingArgs);
  for(i = 0; i < nrRemainingArgs; i++) {fprintf(stderr, " [%d]=%p %s,", i, &nextObjPtr[i], ObjStr(nextObjPtr[i]));}
  fprintf(stderr, "\n");
  }
#endif

  /*
   * The current call-frame of configure uses an obj-frame, such
   * that setvar etc.  are able to access variables like "a" as a
   * local variable.  However, in the init block, we do not like
   * that behavior, since this should look like like a proc body.
   * So we push yet another call-frame without providing the
   * var-frame.
   *
   * The new frame will have the namespace of the caller to avoid
   * the current obj-frame. Nsf_PushFrameCsc() will establish a
   * CMETHOD frame.
   */

  Tcl_Interp_varFramePtr(interp) = varFramePtr->callerVarPtr;
  cscPtr->flags = 0;
  CscInit(cscPtr, object, object->cl /*cl*/, NULL /*cmd*/,
          NSF_CSC_TYPE_PLAIN, 0, NsfGlobalStrings[NSF_CONFIGURE]);
  Nsf_PushFrameCsc(interp, cscPtr, framePtr2);

  if ((paramPtr->flags & (NSF_ARG_INITCMD|NSF_ARG_CMD)) != 0u) {
    /* cscPtr->cmdPtr = NSFindCommand(interp, "::eval"); */
    result = Tcl_EvalObjEx(interp, newValue, TCL_EVAL_DIRECT);

  } else if ((paramPtr->flags & NSF_ARG_ALIAS) != 0u) {
    Tcl_Obj *methodObj, **ovPtr, *ov0;
    static Tcl_Obj *constantObj = NULL;
    const char *methodString;
    int oc = 0;

    /*
     * Restore the variable frame context as found at the original call
     * site of configure(). Note that we do not have to revert this
     * context change when leaving this configure() context because a
     * surrounding [uplevel] will correct the call-stack context for us ...
     */
    if (uplevelVarFramePtr != NULL) {
      Tcl_Interp_varFramePtr(interp) = uplevelVarFramePtr;
    }

    /*
     * Mark the intermittent CSC frame as INACTIVE, so that, e.g.,
     * call-stack traversals seeking active frames ignore it.
     */
    cscPtr->frameType = NSF_CSC_TYPE_INACTIVE;

    /*
     * If parameter option "method=" was given, use it as method name
     */
    methodObj = (paramPtr->method != NULL) ? paramPtr->method : paramPtr->nameObj;
    methodString = ObjStr(methodObj);

    /*fprintf(stderr, "ALIAS %s, nrargs %d converter %p ConvertToNothing %d oc %d\n",
            paramPtr->name, paramPtr->nrArgs, paramPtr->converter,
            paramPtr->converter == ConvertToNothing,
            oc);*/

    if (paramPtr->converter == ConvertToNothing) {
      /*
       * We are using the varargs interface; pass all remaining args into
       * the called method.
       */
      if (newValue == paramPtr->defaultValue) {
        /*
         * Use the default.
         */
        if (Tcl_ListObjGetElements(interp, paramPtr->defaultValue, &oc, &ovPtr) != TCL_OK) {
          goto method_arg_done;
        }
        ov0 = *ovPtr;
        ovPtr ++;
      } else {
        /*
         * Use actual args.
         */
        ov0 = *nextObjPtr;
        /*fprintf(stderr, "ALIAS use actual args oc %d ov0 <%s> nextObjPtr %p %p\n",
          nrRemainingArgs, ObjStr(ov0), nextObjPtr, nextObjPtr+1);*/
        ovPtr = nextObjPtr+1;
        oc = nrRemainingArgs;
      }
    } else {
      /*
       * A simple alias, receives no arg (when noarg was specified) or a
       * single argument (which might be the default value).
       */
      int moc = 1;
      Tcl_Obj **movPtr = NULL;

      ov0 = NULL;
      ovPtr = &constantObj;

      if (Tcl_ListObjGetElements(interp, methodObj, &moc, &movPtr) == TCL_OK) {
        if (moc != 2) {
          oc = 0;
          if (unlikely(moc > 2)) {
            NsfLog(interp, NSF_LOG_WARN, "max 2 words are currently allowed in methodName <%s>", methodString);
          }
        } else {
          oc = 1;
          methodObj = movPtr[0];
          ov0 = movPtr[1];
        }
      }
      if (paramPtr->nrArgs == 1) {
        oc++;
        if (oc == 1) {
          ov0 = newValue;
        } else {
          ovPtr = &newValue;
        }
      }
    }

    /*
     * Check, if we have an object parameter alias for the constructor.
     * Since we require the object system for the current object to
     * determine its object system configuration, we can't do this at
     * parameter compile time.
     */
    if (*initString == *methodString && strcmp(initString, methodString) == 0) {
      result = DispatchInitMethod(interp, object, oc, &ov0, 0);
    } else {

      /*fprintf(stderr, "... call alias %s with methodObj %s.%s oc %d, nrArgs %d '%s'\n",
              paramPtr->name, ObjectName(object), ObjStr(methodObj), oc,
              paramPtr->nrArgs, ObjStr(newValue));*/
#if !defined(NDEBUG)
      if (oc > 2) {
        assert(ovPtr != NULL);
        assert(ovPtr != &constantObj);
        assert(ISOBJ(ovPtr[oc-2]));
      }
#endif
      Tcl_ResetResult(interp);
      result = NsfCallMethodWithArgs(interp, (Nsf_Object*)object, methodObj,
                                     ov0, oc, ovPtr,
                                     NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS);
    }
  } else {

    /* must be NSF_ARG_FORWARD */
    assert((paramPtr->flags & NSF_ARG_FORWARD) != 0u);

    result = ParameterMethodForwardDispatch(interp, object,
                                            paramPtr, newValue, cscPtr);
  }
 method_arg_done:
  /*
   * Pop previously stacked frame for eval context and set the
   * varFramePtr to the previous value.
   */
  Nsf_PopFrameCsc(interp, framePtr2);
  CscListRemove(interp, cscPtr, NULL);
  CscFinish(interp, cscPtr, result, "converter object frame");
  Tcl_Interp_varFramePtr(interp) = varFramePtr;

  /* fprintf(stderr, "NsfOConfigureMethod_ attribute %s evaluated %s => (%d)\n",
     ObjStr(paramPtr->nameObj), ObjStr(newValue), result);*/

  if (likely(result == TCL_OK)) {
    if ((paramPtr->flags & NSF_ARG_CMD) != 0u
        && RUNTIME_STATE(interp)->doKeepcmds
        ) {
      Tcl_Obj *resultObj;

      resultObj = Tcl_ObjSetVar2(interp, NsfGlobalObjs[NSF_ARRAY_CMD], paramPtr->nameObj, newValue, TCL_LEAVE_ERR_MSG);
      if (unlikely(resultObj == NULL)) {
        result = TCL_ERROR;
      }
    }
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * MakeProc --
 *
 *    Define a scripted function via the ObjCmd "proc".
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    Defined function or exception.
 *
 *----------------------------------------------------------------------
 */
static int MakeProc(Tcl_Namespace *nsPtr, NsfAssertionStore *aStore, Tcl_Interp *interp,
         Tcl_Obj *nameObj, Tcl_Obj *args, Tcl_Obj *body, Tcl_Obj *precondition,
         Tcl_Obj *postcondition, NsfObject *defObject, NsfObject *regObject,
         int withPer_object, int withInner_namespace, int checkAlwaysFlag)
  nonnull(1) nonnull(3) nonnull(4) nonnull(5) nonnull(6) nonnull(9);

static int
MakeProc(Tcl_Namespace *nsPtr, NsfAssertionStore *aStore, Tcl_Interp *interp,
         Tcl_Obj *nameObj, Tcl_Obj *args, Tcl_Obj *body, Tcl_Obj *precondition,
         Tcl_Obj *postcondition, NsfObject *defObject, NsfObject *regObject,
         int withPer_object, int withInner_namespace, int checkAlwaysFlag) {
  Tcl_CallFrame frame, *framePtr = &frame;
  const char *methodName = ObjStr(nameObj);
  NsfParsedParam parsedParam;
  Tcl_Obj *ov[4];
  int result;

  nonnull_assert(nsPtr != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(args != NULL);
  nonnull_assert(body != NULL);
  nonnull_assert(defObject != NULL);
  assert(*methodName != ':');

  if (regObject == NULL) {regObject = defObject;}
  /* Check, if we are allowed to redefine the method */
  result = CanRedefineCmd(interp, nsPtr, defObject, methodName, 0);
  if (likely(result == TCL_OK)) {
    /* Yes, we can! ...so obtain an method parameter definitions */
    result = ParamDefsParse(interp, nameObj, args,
                            NSF_DISALLOWED_ARG_METHOD_PARAMETER, 0,
                            &parsedParam);
  }

  if (unlikely(result != TCL_OK)) {
    return result;
  }

  ov[0] = NULL; /*objv[0];*/
  ov[1] = nameObj;

  if (parsedParam.paramDefs != NULL) {
    Nsf_Param *pPtr;
    Tcl_Obj *argList = Tcl_NewListObj(0, NULL);

    for (pPtr = parsedParam.paramDefs->paramsPtr; pPtr->name; pPtr++) {
      if (*pPtr->name == '-') {
        Tcl_ListObjAppendElement(interp, argList, Tcl_NewStringObj(pPtr->name+1, -1));
      } else {
        Tcl_ListObjAppendElement(interp, argList, Tcl_NewStringObj(pPtr->name, -1));
      }
    }
    ov[2] = argList;
    INCR_REF_COUNT(ov[2]);
    /*fprintf(stderr, "final arglist = <%s>\n", ObjStr(argList)); */
    ov[3] = AddPrefixToBody(body, 1, &parsedParam);
  } else { /* no parameter handling needed */
    ov[2] = args;
    ov[3] = AddPrefixToBody(body, 0, &parsedParam);
  }

  Tcl_PushCallFrame(interp, (Tcl_CallFrame *)framePtr, nsPtr, 0);
  /*
   * Create the method in the provided namespace.
   */
  result = Tcl_ProcObjCmd(NULL, interp, 4, ov);

  if (likely(result == TCL_OK)) {
    /* retrieve the defined proc */
    Proc *procPtr = FindProcMethod(nsPtr, methodName);

    if (procPtr != NULL) {
      /* modify the cmd of the proc to set the current namespace for the body */
      if (withInner_namespace == 1) {
        /*
         * Set the namespace of the method as inside of the class.
         */
        if (regObject->nsPtr == NULL) {
          MakeObjNamespace(interp, regObject);
        }
        /*fprintf(stderr, "obj %s\n", ObjectName(defObject));
        fprintf(stderr, "ns %p defObject->ns %p\n", nsPtr, defObject->nsPtr);
        fprintf(stderr, "ns %s defObject->ns %s\n", nsPtr->fullName, defObject->nsPtr->fullName);
        fprintf(stderr, "old %s\n", procPtr->cmdPtr->nsPtr->fullName);*/
        procPtr->cmdPtr->nsPtr = (Namespace *)regObject->nsPtr;
      } else {
        /*
         * Set the namespace of the method to the same namespace the cmd of
         * the defObject has.
         */
        procPtr->cmdPtr->nsPtr = ((Command *)regObject->id)->nsPtr;
      }

      ParamDefsStore(interp, (Tcl_Command)procPtr->cmdPtr, parsedParam.paramDefs, checkAlwaysFlag);
      Tcl_SetObjResult(interp, MethodHandleObj(defObject, withPer_object, methodName));
      result = TCL_OK;
    }
  }
  Tcl_PopCallFrame(interp);

#if defined(NSF_WITH_ASSERTIONS)
  if (result == TCL_OK && (precondition || postcondition)) {
    AssertionAddProc(interp, methodName, aStore, precondition, postcondition);
  }
#endif

  if (parsedParam.paramDefs != NULL) {
    DECR_REF_COUNT(ov[2]);
  }
  DECR_REF_COUNT2("resultBody", ov[3]);

  return result;
}

/*
 *----------------------------------------------------------------------
 * MakeMethod --
 *
 *    Define a scripted method to be defined on defObject and registered on
 *    regObject (if specified). This function handles as well assertions.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    Defined method or exception.
 *
 *----------------------------------------------------------------------
 */
static int MakeMethod(Tcl_Interp *interp, NsfObject *defObject, NsfObject *regObject,
           NsfClass *cl, Tcl_Obj *nameObj, Tcl_Obj *args, Tcl_Obj *body,
           Tcl_Obj *precondition, Tcl_Obj *postcondition,
           int withInner_namespace, int checkAlwaysFlag)
  nonnull(1) nonnull(2) nonnull(5) nonnull(6) nonnull(7);

static int
MakeMethod(Tcl_Interp *interp, NsfObject *defObject, NsfObject *regObject,
           NsfClass *cl, Tcl_Obj *nameObj, Tcl_Obj *args, Tcl_Obj *body,
           Tcl_Obj *precondition, Tcl_Obj *postcondition,
           int withInner_namespace, int checkAlwaysFlag) {
  const char *argsStr = ObjStr(args), *bodyStr = ObjStr(body), *nameStr = ObjStr(nameObj);
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(defObject != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(args != NULL);
  nonnull_assert(body != NULL);

  if (precondition != NULL && postcondition == NULL) {
    return NsfPrintError(interp, "%s method '%s'; when specifying a precondition (%s)"
                         " a postcondition must be specified as well",
                         ClassName(cl), nameStr, ObjStr(precondition));
  }

  if (*argsStr == 0 && *bodyStr == 0) {
    /*
     * Both, args and body are empty strings. This means we should delete the
     * method.
     */
    if (RUNTIME_STATE(interp)->exitHandlerDestroyRound == NSF_EXITHANDLER_OFF) {
      /*
       * Don't delete methods via scripting during shutdown
       */
      result = (cl != NULL) ?
        NsfRemoveClassMethod(interp, (Nsf_Class *)cl, nameStr) :
        NsfRemoveObjectMethod(interp, (Nsf_Object *)defObject, nameStr);
    } else {
      /* fprintf(stderr, "don't delete method %s during shutdown\n", nameStr); */
      result = TCL_OK;
    }

  } else {
#if defined(NSF_WITH_ASSERTIONS)
    NsfAssertionStore *aStore = NULL;
    if (precondition != NULL || postcondition != NULL) {
      if (cl != NULL) {
        NsfClassOpt *opt = NsfRequireClassOpt(cl);
        if (opt->assertions == NULL) {
          opt->assertions = AssertionCreateStore();
        }
        aStore = opt->assertions;
      } else {
        NsfObjectOpt *opt = NsfRequireObjectOpt(defObject);
        if (opt->assertions == NULL) {
          opt->assertions = AssertionCreateStore();
        }
        aStore = opt->assertions;
      }
    }
    result = MakeProc((cl != NULL) ? cl->nsPtr : defObject->nsPtr, aStore,
                      interp, nameObj, args, body, precondition, postcondition,
                      defObject, regObject, cl == NULL, withInner_namespace,
                      checkAlwaysFlag);
#else
    if (precondition != NULL) {
      NsfLog(interp, NSF_LOG_WARN, "Precondition %s provided, but not compiled with assertion enabled",
             ObjStr(precondition));
    } else if (postcondition != NULL) {
      NsfLog(interp, NSF_LOG_WARN, "Postcondition %s provided, but not compiled with assertion enabled",
             ObjStr(postcondition));
    }
    result = MakeProc((cl != NULL) ? cl->nsPtr : defObject->nsPtr, NULL,
                      interp, nameObj, args, body, NULL, NULL,
                      defObject, regObject, cl == NULL, withInner_namespace,
                      checkAlwaysFlag);
#endif
  }

  if (cl != NULL) {
    NsfInstanceMethodEpochIncr("MakeMethod");
    /* could be a filter or filter inheritance ... update filter orders */
    if (FilterIsActive(interp, nameStr)) {
      NsfClasses *subClasses = TransitiveSubClasses(cl);
      if (subClasses != NULL) {
        FilterInvalidateObjOrders(interp, subClasses);
        NsfClassListFree(subClasses);
      }
    }
  } else {
    NsfObjectMethodEpochIncr("MakeMethod");
    /* could be a filter => recompute filter order */
    FilterComputeDefined(interp, defObject);
  }

  return result;
}

/**************************************************************************
 * Begin Definition of nsf::proc (Tcl Procs with Parameter handling)
 **************************************************************************/
/*
 *----------------------------------------------------------------------
 * NsfProcStubDeleteProc --
 *
 *    Tcl_CmdDeleteProc for NsfProcStubs. Is called, whenever a
 *    NsfProcStub is deleted and frees the associated client data.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees client-data
 *
 *----------------------------------------------------------------------
 */
static void
NsfProcStubDeleteProc(ClientData clientData) {
  NsfProcClientData *tcd = clientData;

  /*fprintf(stderr, "NsfProcStubDeleteProc received %p\n", clientData);
    fprintf(stderr, "... procName %s paramDefs %p\n", ObjStr(tcd->procName), tcd->paramDefs);*/

  DECR_REF_COUNT2("procNameObj", tcd->procName);
  if (tcd->cmd != NULL) {
    NsfCommandRelease(tcd->cmd);
  }
  /* tcd->paramDefs is freed by NsfProcDeleteProc() */
  FREE(NsfProcClientData, tcd);
}

/*
 *----------------------------------------------------------------------
 * InvokeShadowedProc --
 *
 *    Call the proc specified in objc/objv; procNameObj should be used
 *    for error messages.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int InvokeShadowedProc(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Command cmd, ParseContext *pcPtr)
  nonnull(1) nonnull(2) nonnull(4) nonnull(3);

static int
InvokeShadowedProc(Tcl_Interp *interp, Tcl_Obj *procNameObj, Tcl_Command cmd, ParseContext *pcPtr) {
  Tcl_Obj *CONST *objv;
  int objc, result;
  const char *fullMethodName = ObjStr(procNameObj);
  Tcl_CallFrame *framePtr;
  Proc *procPtr;
#if defined(NSF_PROFILE)
  struct timeval trt;
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  if (rst->doProfile != 0) {
    gettimeofday(&trt, NULL);
  }
#endif

  nonnull_assert(interp != NULL);
  nonnull_assert(procNameObj != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(pcPtr != NULL);

  objv = pcPtr->full_objv;
  objc = pcPtr->objc+1;

  CheckCStack(interp, "nsfProc", fullMethodName);

  /*
   * The code below is derived from the scripted method dispatch and just
   * slightly adapted to remove object dependencies.
   */

  /*fprintf(stderr, "fullMethodName %s epoch %d\n", fullMethodName, Tcl_Command_cmdEpoch(cmd));*/

  if (Tcl_Command_cmdEpoch(cmd)) {
#if 1
    /*
     * It seems, as someone messed around with the shadowed proc. For now, we
     * give up.
     */
    return NsfPrintError(interp, "command '%s' is epoched", fullMethodName);
#else
    /*
     * We could refetch the command ...
     */

    cmd = Tcl_GetCommandFromObj(interp, procNameObj);
    if (unlikely(cmd == NULL)) {
      return NsfPrintError(interp, "cannot lookup command '%s'", fullMethodName);
    }
    if (unlikely(!CmdIsProc(cmd))) {
      return NsfPrintError(interp, "command '%s' is not a proc", fullMethodName);
    }
    /*
     * ... and update the refCounts
     */
    NsfCommandRelease(tcd->cmd);
    tcd->cmd = cmd;
    NsfCommandPreserve(tcd->cmd);
#endif
  }

#if defined(NSF_PROFILE)
  if (rst->doTrace) {
    NsfProfileTraceCallAppend(interp, fullMethodName);
  }
#endif

  procPtr = (Proc *)Tcl_Command_objClientData(cmd);
  result = TclPushStackFrame(interp, &framePtr,
                             (Tcl_Namespace *) procPtr->cmdPtr->nsPtr,
                             (FRAME_IS_PROC));

  if (likely(result == TCL_OK)) {
    unsigned int dummy = 0;
    result = ByteCompiled(interp, &dummy, procPtr, fullMethodName);
  }
  if (unlikely(result != TCL_OK)) {
    /* todo: really? error msg? */
    return result;
  }

  Tcl_CallFrame_objc(framePtr) = objc;
  Tcl_CallFrame_objv(framePtr) = objv;
  Tcl_CallFrame_procPtr(framePtr) = procPtr;

#if defined(NRE)
  /*fprintf(stderr, "CALL TclNRInterpProcCore proc '%s' %s nameObj %p %s\n",
    ObjStr(objv[0]), fullMethodName, procNameObj, ObjStr(procNameObj));*/
  Tcl_NRAddCallback(interp, ProcDispatchFinalize,
                    (ClientData)fullMethodName, pcPtr,
# if defined(NSF_PROFILE)
                    (ClientData)(unsigned long)trt.tv_usec,
                    (ClientData)(unsigned long)trt.tv_sec
# else
                    NULL,
                    NULL
# endif
                    );
  result = TclNRInterpProcCore(interp, procNameObj, 1, &MakeProcError);
#else
  {
  ClientData data[4] = {
    (ClientData)fullMethodName,
    pcPtr,
# if defined(NSF_PROFILE)
    (ClientData)(unsigned long)trt.tv_usec,
    (ClientData)(unsigned long)trt.tv_sec
# else
    NULL,
    NULL
# endif
  };
  result = TclObjInterpProcCore(interp, procNameObj, 1, &MakeProcError);
  result = ProcDispatchFinalize(data, interp, result);
  }
#endif
  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfProcStub --
 *
 *    Tcl_ObjCmdProc implementing Proc Stubs. This function processes
 *    the argument list in accordance with the parameter definitions
 *    and calls in case of success the shadowed proc.
 *
 * Results:
 *    Tcl return code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
int NsfProcStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(2) nonnull(4) nonnull(1);

int
NsfProcStub(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  NsfProcClientData *tcd;
  int result;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  tcd = clientData;

  /*fprintf(stderr, "NsfProcStub %s is called, tcd %p\n", ObjStr(objv[0]), tcd);*/

  if (likely(tcd->paramDefs && tcd->paramDefs->paramsPtr)) {
    ParseContext *pcPtr = (ParseContext *) NsfTclStackAlloc(interp, sizeof(ParseContext), "parse context");
#if 0
    /* i see no difference from tcl */
    ALLOC_ON_STACK(Tcl_Obj*, objc, tov);

    /*
     * We have to substitute the first element of objv with the name
     * of the function to be called. Since objv is immutable, we have
     * to copy the full argument vector and replace the element on
     * position [0]
     */
    memcpy(tov+1, objv+1, sizeof(Tcl_Obj *)*(objc-1));
    tov[0] = tcd->procName;
#endif
    /* If the argument parsing is ok, the shadowed proc will be called */
    result = ProcessMethodArguments(pcPtr, interp, NULL,
                                    tcd->checkAlwaysFlag|NSF_ARGPARSE_FORCE_REQUIRED,
                                    tcd->paramDefs, objv[0],
                                    objc, objv);

    if (likely(result == TCL_OK)) {
      result = InvokeShadowedProc(interp, tcd->procName, tcd->cmd, pcPtr);
    } else {
      /*Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
      fprintf(stderr, "NsfProcStub: incorrect arguments (%s)\n", ObjStr(resultObj));*/
      ParseContextRelease(pcPtr);
      NsfTclStackFree(interp, pcPtr, "release parse context");
    }

    /*FREE_ON_STACK(Tcl_Obj *, tov);*/
  } else {
    fprintf(stderr, "no parameters\n");
    assert(0); /* should never happen */
    result = TCL_ERROR;
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfProcAdd --
 *
 *    Add a command for implementing a Tcl proc with next scripting
 *    parameter handling.
 *
 *    For the time being, this function adds two things, (a) a Tcl cmd
 *    functioning as a stub for the argument processing (in accordance
 *    with the parameter definitions) and (b) the shadowed Tcl proc
 *    with a mutated name.
 *
 *    TODO: the current 1 cmd + 1 proc implementation is not robust
 *    against renaming and partial deletions (deletion of the
 *    stub).
 *
 * Results:
 *    Tcl return code.
 *
 * Side effects:
 *    Adding one Tcl command and one Tcl proc
 *
 *----------------------------------------------------------------------
 */
static int NsfProcAdd(Tcl_Interp *interp, NsfParsedParam *parsedParamPtr,
           const char *procName, Tcl_Obj *body,
           int with_ad, int with_checkAlways) nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
NsfProcAdd(Tcl_Interp *interp, NsfParsedParam *parsedParamPtr,
           const char *procName, Tcl_Obj *body,
           int with_ad, int with_checkAlways) {
  NsfParamDefs *paramDefs;
  Nsf_Param *paramPtr;
  NsfProcClientData *tcd;
  Tcl_Namespace *cmdNsPtr;
  Tcl_Obj *argList, *procNameObj, *ov[4];
  Tcl_DString ds, *dsPtr = &ds;
  int result, checkAlwaysFlag;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(parsedParamPtr != NULL);
  nonnull_assert(procName != NULL);
  nonnull_assert(body != NULL);

  Tcl_DStringInit(dsPtr);

  /*
   * Create a fully qualified procName
   */
  if (*procName != ':') {
    DStringAppendQualName(dsPtr, Tcl_GetCurrentNamespace(interp), procName);
    procName = Tcl_DStringValue(dsPtr);
  }
  /*
   * Create first the ProcStub to obtain later its namespace, which is
   * needed as the inner namespace of the shadowed proc.
   */
  tcd = NEW(NsfProcClientData);
  cmd = Tcl_CreateObjCommand(interp, procName, NsfProcStub,
                             tcd, NsfProcStubDeleteProc);
  if (unlikely(cmd == NULL)) {
    /*
     * For some reason, the command could not be created. Let us hope,
     * we have a useful error message.
     */
    Tcl_DStringFree(dsPtr);
    FREE(NsfProcClientData, tcd);
    return TCL_ERROR;
  }

  checkAlwaysFlag = (with_checkAlways != 0) ? NSF_ARGPARSE_CHECK : 0;

  cmdNsPtr = Tcl_Command_nsPtr(cmd);

  /*
   * Storing param defs is actually not needed to be stored, since the stub
   * receives paramters + flag via client data... but it is needed for
   * introspection.
   */
  paramDefs = parsedParamPtr->paramDefs;
  ParamDefsStore(interp, cmd, paramDefs, checkAlwaysFlag);

  /*fprintf(stderr, "NsfProcAdd procName '%s' define cmd '%s' %p in namespace %s\n",
    procName, Tcl_GetCommandName(interp, cmd), cmd, cmdNsPtr->fullName);*/

  /*
   * Let us create the shadowed Tcl proc, which is stored under
   * ::nsf::procs::*. First build the fully qualified name
   * procNameObj.
   */
  Tcl_DStringSetLength(dsPtr, 0);
  Tcl_DStringAppend(dsPtr, "::nsf::procs", -1);
  DStringAppendQualName(dsPtr, cmdNsPtr, Tcl_GetCommandName(interp, cmd));
  procNameObj = Tcl_NewStringObj(Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));

  INCR_REF_COUNT2("procNameObj", procNameObj); /* will be freed, when NsfProcStub is deleted */

  /*
   * Make sure to create the target namespace under "::nsf::procs::", if
   * it does not exist.
   */
  {
    Namespace *nsPtr, *dummy1Ptr, *dummy2Ptr;
    const char *dummy;
    /* create the target namespace, if it does not exist */
    TclGetNamespaceForQualName(interp, ObjStr(procNameObj), NULL, TCL_CREATE_NS_IF_UNKNOWN,
                               &nsPtr, &dummy1Ptr,
                               &dummy2Ptr, &dummy);
  }

  /*
   * Create the client data, which links the stub cmd with the proc.
   */
  tcd->procName = procNameObj;
  tcd->paramDefs = paramDefs;
  tcd->with_ad = with_ad;
  tcd->checkAlwaysFlag = checkAlwaysFlag;
  tcd->cmd = NULL;

  /*fprintf(stderr, "NsfProcAdd %s tcd %p paramdefs %p\n",
    ObjStr(procNameObj), tcd, tcd->paramDefs);*/

  /*
   * Build an argument list for the shadowed proc.
   */
  argList = Tcl_NewListObj(0, NULL);
  INCR_REF_COUNT(argList);

  for (paramPtr = paramDefs->paramsPtr; paramPtr->name; paramPtr++) {
    if (*paramPtr->name == '-') {
      Tcl_Obj *varNameObj = Tcl_NewStringObj(paramPtr->name+1, -1);
      /*
       * If we have the -ad (for ars digita) flag set, we provide the
       * OpenACS semantics. This is (a) to use the name "boolean" for
       * a switch and (b) to name the automatic variable with the
       * prefix "_p".
       */
      if (with_ad && paramPtr->converter == Nsf_ConvertToBoolean && paramPtr->nrArgs == 1) {
        /*fprintf(stderr, "... ad handling: proc %s param %s type %s nrargs %d default %p\n",
          procName, paramPtr->name, paramPtr->type, paramPtr->nrArgs, paramPtr->defaultValue);*/
        paramPtr->nrArgs = 0;
        /*paramPtr->converter = Nsf_ConvertToSwitch;*/
        Tcl_AppendToObj(varNameObj, "_p", 2);
        if (paramPtr->defaultValue == NULL) {
          paramPtr->defaultValue = Tcl_NewBooleanObj(0);
          INCR_REF_COUNT(paramPtr->defaultValue);
        }
      }
      Tcl_ListObjAppendElement(interp, argList, varNameObj);
    } else {
      Tcl_ListObjAppendElement(interp, argList, Tcl_NewStringObj(paramPtr->name, -1));
    }
  }
  ov[0] = NULL;
  ov[1] = procNameObj;
  ov[2] = argList;
  ov[3] = AddPrefixToBody(body, 1, parsedParamPtr);

  /*fprintf(stderr, "NsfProcAdd define proc %s arglist '%s'\n",
    ObjStr(ov[1]), ObjStr(ov[2])); */

  result = Tcl_ProcObjCmd(0, interp, 4, ov);
  DECR_REF_COUNT(argList);
  DECR_REF_COUNT2("resultBody", ov[3]);

  if (likely(result == TCL_OK)) {
    /*
     * The shadowed proc was created successfully. Retrieve the
     * defined proc and set its namespace to the namespace of the stub
     * cmd
     */
    Tcl_Command procCmd = Tcl_GetCommandFromObj(interp, procNameObj);
    assert(procCmd != NULL);
    ((Command *)procCmd)->nsPtr = (Namespace *)cmdNsPtr;
    tcd->cmd = procCmd;
    NsfCommandPreserve(tcd->cmd);

  } else {
    /*
     * We could not define the shadowed proc. In this case, cleanup by
     * removing the stub cmd.
     */
    Tcl_DeleteCommandFromToken(interp, cmd);
  }

  Tcl_DStringFree(dsPtr);
  return result;
}

/*
 *----------------------------------------------------------------------
 * ProcessMethodArguments --
 *
 *    Process the arguments provided to a method call. It parses the argument
 *    vector objv, disallows certain parameter types and updates the parse
 *    context.
 *
 * Results:
 *    Tcl return code.
 *
 * Side effects:
 *    Updates parameter context
 *
 *----------------------------------------------------------------------
 */
static int
ProcessMethodArguments(ParseContext *pcPtr, Tcl_Interp *interp,
                       NsfObject *object, int processFlags, NsfParamDefs *paramDefs,
                       Tcl_Obj *methodNameObj, int objc, Tcl_Obj *CONST objv[]) {
  int result;
  CallFrame frame, *framePtr = &frame;

  nonnull_assert(pcPtr != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(paramDefs != NULL);
  nonnull_assert(methodNameObj != NULL);
  nonnull_assert(objv != NULL);

  if (object != NULL && (processFlags & NSF_ARGPARSE_METHOD_PUSH) != 0u ) {
    Nsf_PushFrameObj(interp, object, framePtr);
  }

#if 0
  {int i;
    fprintf(stderr, "ProcessMethodArguments before ArgumentParse %s (flags %.6x objc %d): ", ObjStr(methodNameObj), processFlags, objc);
    for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
    fprintf(stderr, "\n");
  }
#endif

  result = ArgumentParse(interp, objc, objv, object, methodNameObj,
                         paramDefs->paramsPtr, paramDefs->nrParams, paramDefs->serial,
                         processFlags|RUNTIME_STATE(interp)->doCheckArguments,
                         pcPtr);
#if 0
  {
    int i, fromArg, toArg;
    fprintf(stderr, "ProcessMethodArguments after ArgumentParse %s pcPtr->objc %d result %d\n",
            ObjStr(methodNameObj), pcPtr->objc, result);
    if (result == TCL_OK) {
      if ((processFlags & NSF_ARGPARSE_START_ZERO) != 0u) {
        fromArg = 0; toArg = pcPtr->objc;
      } else {
        fromArg = 1; toArg = pcPtr->objc;
      }
      for (i = fromArg; i < toArg; i++) {
        fprintf(stderr, "... pcPtr %p [%d] obj %p refCount %d (%s) flags %.6x & %p\n",
                pcPtr, i,
                pcPtr->objv[i] ? pcPtr->objv[i] : NULL,
                pcPtr->objv[i] ? pcPtr->objv[i]->refCount : -1,
                pcPtr->objv[i] ? ObjStr(pcPtr->objv[i]) : "(null)", pcPtr->flags[i], &(pcPtr->flags[i]));
      }
    }
  }
#endif

  if (object != NULL && ((processFlags & NSF_ARGPARSE_METHOD_PUSH) != 0u)) {
    Nsf_PopFrameObj(interp, framePtr);
  }

  /*
   * Set objc of the parse context to the number of defined parameters.
   * pcPtr->objc and paramDefs->nrParams will be equivalent in cases
   * where argument values are passed to the call in absence of var
   * args ('args'). Treating "args is more involved (see below).
   */

  if (unlikely(result != TCL_OK)) {
    return result;
  }

  if (pcPtr->varArgs == 1) {
    /*
     * The last argument was "args".
     */
    int elts = objc - pcPtr->lastObjc;

    if (elts == 0) {
      /*
       * No arguments were passed to "args".  We simply decrement objc.
       */
      pcPtr->objc--;
    } else if (elts > 1) {
      /*
       * Multiple arguments were passed to "args".  pcPtr->objv is
       * pointing to the first of the var args. We have to copy the
       * remaining actual argument vector objv to the parse context.
       */

      /*NsfPrintObjv("actual:  ", objc, objv);*/
      ParseContextExtendObjv(pcPtr, paramDefs->nrParams, elts-1, objv + 1 + pcPtr->lastObjc);
    } else {
      /*
       * A single argument was passed to "args". There is no need to
       * mutate the pcPtr->objv, because this has been achieved in
       * ArgumentParse (i.e., pcPtr->objv[i] contains this element).
       */
    }
  }

  return TCL_OK;
}
/**************************************************************************
 * End Definition of nsf::proc  (Tcl Procs with Parameter handling)
 **************************************************************************/

/*
 *----------------------------------------------------------------------
 * ForwardCmdDeleteProc --
 *
 *    This Tcl_CmdDeleteProc is called, when a forward method is deleted
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees client data of the setter command.
 *
 *----------------------------------------------------------------------
 */

static void
ForwardCmdDeleteProc(ClientData clientData) {
  ForwardCmdClientData *tcd;

  nonnull_assert(clientData != NULL);

  tcd = (ForwardCmdClientData *)clientData;
  if (tcd->cmdName != NULL)     {DECR_REF_COUNT(tcd->cmdName);}
  if (tcd->subcommands != NULL) {DECR_REF_COUNT(tcd->subcommands);}
#if defined(NSF_FORWARD_WITH_ONERROR)
  if (tcd->onerror != NULL)     {DECR_REF_COUNT(tcd->onerror);}
#endif
  if (tcd->prefix != NULL)      {DECR_REF_COUNT(tcd->prefix);}
  if (tcd->args != NULL)        {DECR_REF_COUNT(tcd->args);}
  FREE(ForwardCmdClientData, tcd);
}


/*
 *----------------------------------------------------------------------
 * SetterCmdDeleteProc --
 *
 *    This Tcl_CmdDeleteProc is called, when a setter method is deleted
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees client data of the setter command.
 *
 *----------------------------------------------------------------------
 */
static void SetterCmdDeleteProc(ClientData clientData) nonnull(1);

static void
SetterCmdDeleteProc(ClientData clientData) {
  SetterCmdClientData *setterClientData;

  nonnull_assert(clientData != NULL);

  setterClientData = (SetterCmdClientData *)clientData;
  if (setterClientData->paramsPtr != NULL) {
    ParamsFree(setterClientData->paramsPtr);
  }
  FREE(SetterCmdClientData, setterClientData);
}


/*
 *----------------------------------------------------------------------
 * AliasCmdDeleteProc --
 *
 *    This Tcl_CmdDeleteProc is called, when an alias method is deleted
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees client data of the setter command.
 *
 *----------------------------------------------------------------------
 */
static void AliasCmdDeleteProc(ClientData clientData) nonnull(1);

static void
AliasCmdDeleteProc(ClientData clientData) {
  AliasCmdClientData *tcd;

  nonnull_assert(clientData != NULL);

  /*
   * Since we just get the clientData, we have to obtain interp,
   * object, methodName and per-object from tcd; the obj might be
   * deleted already. We need as well at least still the global
   * namespace.
   */
  tcd = (AliasCmdClientData *)clientData;
  if (tcd->interp != NULL &&
      ((Interp *)(tcd->interp))->globalNsPtr != NULL &&
      RUNTIME_STATE(tcd->interp)->exitHandlerDestroyRound != NSF_EXITHANDLER_ON_PHYSICAL_DESTROY) {
    const char *methodName = Tcl_GetCommandName(tcd->interp, tcd->aliasCmd);
    AliasDelete(tcd->interp, tcd->cmdName, methodName, tcd->class == NULL);
  }

  /*fprintf(stderr, "AliasCmdDeleteProc aliasedCmd %p\n", tcd->aliasedCmd);*/
  if (tcd->cmdName != NULL)     {DECR_REF_COUNT(tcd->cmdName);}
  if (tcd->aliasedCmd != NULL) {

#if defined(WITH_IMPORT_REFS)
    ImportRef *refPtr, *prevPtr = NULL;
    Command *aliasedCmd = (Command *)(tcd->aliasedCmd);

    /*fprintf(stderr, "AliasCmdDeleteProc aliasedCmd %p epoch %d refCount %d\n",
      aliasedCmd, Tcl_Command_cmdEpoch(tcd->aliasedCmd), aliasedCmd->refCount);*/
    /*
     * Clear the aliasCmd from the imported-ref chain of the aliased
     * (or real) cmd.  This widely resembles what happens in the
     * DeleteImportedCmd() (see tclNamesp.c), however, as we do not
     * provide for ImportedCmdData client data etc., we cannot
     * directly use it.
     */
    for (refPtr = aliasedCmd->importRefPtr; refPtr; refPtr = refPtr->nextPtr) {
      if (refPtr->importedCmdPtr == (Command *) tcd->aliasCmd) {
        if (prevPtr == NULL) {
          aliasedCmd->importRefPtr = refPtr->nextPtr;
        } else {
          prevPtr->nextPtr = refPtr->nextPtr;
        }
        ckfree((char *) refPtr);
        break;
      }
      prevPtr = refPtr;
    }
#endif
    NsfCommandRelease(tcd->aliasedCmd);
  }
  FREE(AliasCmdClientData, tcd);
}

/*
 *----------------------------------------------------------------------
 * GetMatchObject --
 *
 *    Helper method used by nsfAPI.h and the info methods to check if the the
 *    Tcl_Obj patternObj was provided and can be looked up. If this is the
 *    case, wild card matching etc. does not have to be performed, but just
 *    the properties of the object have to be tested.
 *
 * Results:
 *    0 or 1 or -1, potentially the matchObject (when 0 is returned)
 *    0: we have wild-card characters, iterate to get matches
 *    1: we have an existing object
 *   -1: we no wild-card characters and a non-existing object
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int
GetMatchObject(Tcl_Interp *interp, Tcl_Obj *patternObj, Tcl_Obj *origObj,
                NsfObject **matchObjectPtr, const char **patternPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(matchObjectPtr != NULL);
  nonnull_assert(patternPtr != NULL);

  if (patternObj != NULL) {
    *patternPtr = ObjStr(patternObj);
    if (TclObjIsNsfObject(interp, patternObj, matchObjectPtr)) {
      return 1;
    }
    if (patternObj == origObj && **patternPtr != ':') {
      return -1;
    }
  }

  return 0;
}


/*
 *----------------------------------------------------------------------
 * ForwardProcessOptions --
 *
 *    Process the options provided by the forward method and turn these into
 *    the ForwardCmdClientData structure.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Allocated and initialized ForwardCmdClientData
 *
 *----------------------------------------------------------------------
 */

static int
ForwardProcessOptions(Tcl_Interp *interp, Tcl_Obj *nameObj,
                      Tcl_Obj *withDefault,
                      int withEarlybinding,
                      Tcl_Obj *withOnerror,
                      Tcl_Obj *withMethodprefix,
                      int withFrame,
                      int withVerbose,
                      Tcl_Obj *target, int objc, Tcl_Obj * CONST objv[],
                       ForwardCmdClientData **tcdPtr) {
  ForwardCmdClientData *tcd;
  int i, result = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(objv != NULL);

  tcd = NEW(ForwardCmdClientData);
  memset(tcd, 0, sizeof(ForwardCmdClientData));

  if (withDefault != 0) {
    Tcl_DString ds, *dsPtr = &ds;
    DSTRING_INIT(dsPtr);
    Tcl_DStringAppend(dsPtr, "%1 {", 4);
    Tcl_DStringAppend(dsPtr, ObjStr(withDefault), -1);
    Tcl_DStringAppend(dsPtr, "}", 1);
    NsfDeprecatedCmd(interp, "forward option","-default ...", Tcl_DStringValue(dsPtr));
    DSTRING_FREE(dsPtr);

    tcd->subcommands = withDefault;
    result = Tcl_ListObjLength(interp, withDefault, &tcd->nr_subcommands);
    INCR_REF_COUNT(tcd->subcommands);
  }
  if (withMethodprefix != 0) {
    tcd->prefix = withMethodprefix;
    INCR_REF_COUNT(tcd->prefix);
  }
#if defined(NSF_FORWARD_WITH_ONERROR)
  if (withOnerror != 0) {
    tcd->onerror = withOnerror;
    INCR_REF_COUNT(tcd->onerror);
  }
#endif
  tcd->frame = withFrame;
  tcd->verbose = withVerbose;
  tcd->needobjmap = 0;
  tcd->cmdName = target;
  /*fprintf(stderr, "...forwardprocess objc %d, cmdName %p %s\n", objc, target, ObjStr(target));*/

  for (i = 0; i < objc; i++) {
    const char *element = ObjStr(objv[i]);
    /*fprintf(stderr, "... [%d] forwardprocess element '%s'\n", i, element);*/
    tcd->needobjmap |= (*element == '%' && *(element+1) == '@');
    tcd->hasNonposArgs |= (*element == '%' && *(element+1) == '-');
    if (tcd->args == NULL) {
      tcd->args = Tcl_NewListObj(1, &objv[i]);
      tcd->nr_args++;
      INCR_REF_COUNT(tcd->args);
    } else {
      Tcl_ListObjAppendElement(interp, tcd->args, objv[i]);
      tcd->nr_args++;
    }
  }

  if (tcd->cmdName == NULL) {
    tcd->cmdName = nameObj;
  }

  /*fprintf(stderr, "+++ cmdName = %s, args = %s, # = %d\n",
    ObjStr(tcd->cmdName), (tcd->args != NULL) ?ObjStr(tcd->args):"NULL", tcd->nr_args);*/

  if (tcd->frame == FrameObjectIdx) {
    /*
     * When we evaluating objscope, and define ...
     *     o forward append -frame object append
     *  a call to
     *     o append ...
     *  would lead to a recursive call; so we add the appropriate namespace.
     */
    const char *nameString = ObjStr(tcd->cmdName);
    if (!isAbsolutePath(nameString)) {
      tcd->cmdName = NameInNamespaceObj(nameString, CallingNameSpace(interp));
      /*fprintf(stderr, "+++ name %s not absolute, therefore qualifying %s\n", nameString,
        ObjStr(tcd->cmdName));*/
    }
  }
  INCR_REF_COUNT(tcd->cmdName);

  if (withEarlybinding != 0) {
    Tcl_Command cmd = Tcl_GetCommandFromObj(interp, tcd->cmdName);
    if (cmd == NULL) {
      result = NsfPrintError(interp, "cannot lookup command '%s'", ObjStr(tcd->cmdName));
      goto forward_process_options_exit;
    }
    if (CmdIsNsfObject(cmd)     /* don't do direct invoke on nsf objects */
        || Tcl_Command_objProc(cmd) == TclObjInterpProc  /* don't do direct invoke on tcl procs */
        ) {
      /* silently ignore earlybinding flag */
      tcd->objProc = NULL;
    } else {
      tcd->objProc = Tcl_Command_objProc(cmd);
      tcd->clientData = Tcl_Command_objClientData(cmd);
    }
  }

  tcd->passthrough = tcd->args == NULL && *(ObjStr(tcd->cmdName)) != '%' && tcd->objProc;

 forward_process_options_exit:
  /*fprintf(stderr, "forward args = %p, name = '%s'\n", tcd->args, ObjStr(tcd->cmdName));*/
  if (likely(result == TCL_OK)) {
    *tcdPtr = tcd;
  } else {
    ForwardCmdDeleteProc(tcd);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * StripBodyPrefix --
 *
 *    Strip the prefix of the body, which might have been added by nsf.
 *
 * Results:
 *    The string of the body without the prefix.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static const char * StripBodyPrefix(const char *body) nonnull(1);

static const char *
StripBodyPrefix(const char *body) {

  nonnull_assert(body != NULL);

  if (strncmp(body, "::nsf::__unset_unknown_args\n", 28) == 0) {
    body += 28;
  }
  return body;
}


/*
 *----------------------------------------------------------------------
 * AddSlotObjects --
 *
 *    Compute the slot objects (children of the slot container) for a provided
 *    object. The objects can be filtered via a pattern.
 *
 * Results:
 *    The function appends results to the provide listObj
 *
 * Side effects:
 *    Might add as well to the hash-table to avoid duplicates.
 *
 *----------------------------------------------------------------------
 */
static void AddSlotObjects(Tcl_Interp *interp, NsfObject *parent, const char *prefix,
                           Tcl_HashTable *slotTablePtr, int withSource, NsfClass *type,
                           const char *pattern, Tcl_Obj *listObj)
  nonnull(1) nonnull(2) nonnull(3) nonnull(8);

static void
AddSlotObjects(Tcl_Interp *interp, NsfObject *parent, const char *prefix,
               Tcl_HashTable *slotTablePtr,
               int withSource, NsfClass *type, const char *pattern,
               Tcl_Obj *listObj) {
  NsfObject *slotContainerObject;
  Tcl_DString ds, *dsPtr = &ds;
  int isFullQualPattern = (pattern && *pattern == ':' && *(pattern+1) == ':');

  nonnull_assert(interp != NULL);
  nonnull_assert(parent != NULL);
  nonnull_assert(prefix != NULL);
  nonnull_assert(listObj != NULL);

  /*fprintf(stderr, "AddSlotObjects parent %s prefix %s type %p %s\n",
    ObjectName(parent), prefix, type, (type != NULL) ? ClassName(type) : "");*/

  DSTRING_INIT(dsPtr);
  Tcl_DStringAppend(dsPtr, ObjectName(parent), -1);
  Tcl_DStringAppend(dsPtr, prefix, -1);
  slotContainerObject = GetObjectFromString(interp, Tcl_DStringValue(dsPtr));

  if (slotContainerObject != NULL && slotContainerObject->nsPtr
      && ((slotContainerObject->flags & NSF_IS_SLOT_CONTAINER) != 0u)) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry *hPtr;
    Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(slotContainerObject->nsPtr);
    Tcl_Command cmd;

    hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch);
    for (; hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      const char *key = Tcl_GetHashKey(cmdTablePtr, hPtr);
      NsfObject *childObject;

      if (slotTablePtr != NULL) {
        int new;
        /*
         * Check, if we have and entry with this key already processed. We
         * never want to report shadowed entries.
         */
        Tcl_CreateHashEntry(slotTablePtr, key, &new);
        if (new == 0) {
          continue;
        }
      }

      /*
       * Obtain the childObject
       */
      cmd = (Tcl_Command) Tcl_GetHashValue(hPtr);
      childObject = NsfGetObjectFromCmdPtr(cmd);

      /*
       * Report just the already fully initialized slot objects, not the one
       * being right now created.
       */
      if (childObject == NULL || (childObject->flags & NSF_INIT_CALLED) == 0u) {
        /* fprintf(stderr, "....... key %s unfinished\n", key);*/
        continue;
      }

      /*
       * Check the pattern.
       */
      if (pattern != NULL) {
        int isMatch;
        /*
         * If the pattern looks like fully qualified, we match against the
         * fully qualified name.
         */

        if (isFullQualPattern == 1) {
          isMatch = Tcl_StringMatch(ObjectName(childObject), pattern);
        } else {
          /*
           * do we have a mangled name of a private property/variable?
           */
          if (*key == '_' && *(key+1) == '_' && *(key+2) == '_' && *(key+3) == '_') {
            Tcl_Obj *value = Nsf_ObjGetVar2((Nsf_Object *)childObject, interp,
                                            NsfGlobalObjs[NSF_SETTERNAME], NULL, 0);
            isMatch = (value != NULL) ? Tcl_StringMatch(ObjStr(value), pattern) : 0;

            /*fprintf(stderr, "pattern <%s> isFullQualPattern %d child %s key %s %p <%s> match %d\n",
              pattern, isFullQualPattern, ObjectName(childObject), key,
              value, (value != NULL) ? ObjStr(value) : "", match);*/
          } else {
            isMatch = Tcl_StringMatch(key, pattern);
          }
        }
        if (isMatch == 0) {
          continue;
        }
      }

      /*
       * Check, if the entry is from the right type
       */
      if (type && !IsSubType(childObject->cl, type)) {
        continue;
      }

      /*
       * Add finally the entry to the returned list.
       */
      Tcl_ListObjAppendElement(interp, listObj, childObject->cmdName);
    }
  }
  DSTRING_FREE(dsPtr);
}

/*
 *----------------------------------------------------------------------
 * FindCalledClass --
 *
 *    Find the called class of the called proc on the callstack.
 *
 * Results:
 *    NsfClass * or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfClass *FindCalledClass(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static NsfClass *
FindCalledClass(Tcl_Interp *interp, NsfObject *object) {
  NsfCallStackContent *cscPtr = CallStackGetTopFrame0(interp);
  const char *methodName;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (cscPtr->frameType == NSF_CSC_TYPE_PLAIN) {
    return cscPtr->cl;
  }
  if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER) {
    methodName = MethodName(cscPtr->filterStackEntry->calledProc);
  } else if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_MIXIN && object->mixinStack) {
    methodName = Tcl_GetCommandName(interp, cscPtr->cmdPtr);
  } else {
    return NULL;
  }
  if (object->nsPtr != NULL) {
    cmd = FindMethod(object->nsPtr, methodName);
    if (cmd != NULL) {
      /* we called an object specific method */
      return NULL;
    }
  }

  return SearchCMethod(object->cl, methodName, &cmd);
}

/*
 * Next Primitive Handling
 */
/*
 *----------------------------------------------------------------------
 * NextSearchMethod --
 *
 *    Determine the method to be called via "next". The function returns on
 *    success the found cmd and information like method name, was it from a mixin, filter,
 *    or was the end of the filter chain reached.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int NextSearchMethod(NsfObject *object, Tcl_Interp *interp, NsfCallStackContent *cscPtr,
                 NsfClass **clPtr, const char **methodNamePtr, Tcl_Command *cmdPtr,
                 int *isMixinEntry, int *isFilterEntry,
                 int *endOfFilterChain, Tcl_Command *currentCmdPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5) nonnull(6) nonnull(7) nonnull(8) nonnull(9) nonnull(10);

NSF_INLINE static int
NextSearchMethod(NsfObject *object, Tcl_Interp *interp, NsfCallStackContent *cscPtr,
                 NsfClass **clPtr, const char **methodNamePtr, Tcl_Command *cmdPtr,
                 int *isMixinEntry, int *isFilterEntry,
                 int *endOfFilterChain, Tcl_Command *currentCmdPtr) {
  int endOfChain = 0, objflags;

  nonnull_assert(object != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(cscPtr != NULL);
  nonnull_assert(clPtr != NULL);
  nonnull_assert(methodNamePtr != NULL);
  nonnull_assert(cmdPtr != NULL);
  nonnull_assert(isMixinEntry != NULL);
  nonnull_assert(isFilterEntry != NULL);
  nonnull_assert(endOfFilterChain != NULL);
  nonnull_assert(currentCmdPtr != NULL);

  /*fprintf(stderr, "NextSearchMethod for %s called with cl %p\n", *methodNamePtr, *clPtr);*/

  /*
   *  Next in filters
   */

  objflags = object->flags; /* avoid stalling */
  if ((objflags & NSF_MIXIN_ORDER_VALID) == 0u) {
    MixinComputeDefined(interp, object);
    objflags = object->flags; /* avoid stalling */
  }

  if ((objflags & NSF_FILTER_ORDER_VALID) != 0u
      && object->filterStack
      && object->filterStack->currentCmdPtr) {
    *cmdPtr = FilterSearchProc(interp, object, currentCmdPtr, clPtr);

    /*fprintf(stderr, "FilterSearchProc returned cmd %p\n", *cmdPtr);
      NsfShowStack(interp);*/

    if (*cmdPtr == NULL) {
      if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER) {
        /*
         * Reset the information to the values of method, clPtr
         * to the values they had before calling the filters.
         */
        *methodNamePtr = MethodName(object->filterStack->calledProc);
        endOfChain = 1;
        *endOfFilterChain = 1;
        *clPtr = NULL;
        /*fprintf(stderr, "EndOfChain resetting cl\n");*/
      }
    } else {
      *methodNamePtr = (char *) Tcl_GetCommandName(interp, *cmdPtr);
      *endOfFilterChain = 0;
      *isFilterEntry = 1;
      return TCL_OK;
    }
  }

  /*
   *  Next in Mixins requires that we have already a mixinStack, and the
   *  current frame is not a plain frame.
   */
  assert((objflags & NSF_MIXIN_ORDER_VALID) != 0u);

  if ((object->mixinStack != NULL) && cscPtr->frameType) {
    int result = MixinSearchProc(interp, object, *methodNamePtr, NULL,
                                 clPtr, currentCmdPtr, cmdPtr);

    /* fprintf(stderr, "next in mixins %s frameType %.6x\n", *methodNamePtr, cscPtr->frameType); */

    if (unlikely(result != TCL_OK)) {
      return result;
    }

    if (*cmdPtr == NULL) {
      if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_MIXIN) {
        endOfChain = 1;
        *clPtr = NULL;
      }
    } else {
      *isMixinEntry = 1;
      return TCL_OK;
    }
  }

  /*fprintf(stderr, "nextsearch: object %s nsPtr %p endOfChain %d\n",
    ObjectName(object), object->nsPtr, endOfChain);*/

  /*
   * Otherwise: normal method dispatch
   *
   * If we are already in the precedence ordering, then advance
   * past our last point; otherwise (if clPtr == NULL) begin from the start.
   *
   * When a mixin or filter chain reached its end, we have to check for
   * fully qualified method names and search the obj-specific methods as well.
   */
  if (endOfChain == 1) {
    if (**methodNamePtr == ':') {
      *cmdPtr = Tcl_FindCommand(interp, *methodNamePtr, NULL, TCL_GLOBAL_ONLY);
      /* fprintf(stderr, "NEXT found absolute cmd %s => %p\n", *methodNamePtr, *cmdPtr); */
    } else if (object->nsPtr != NULL) {
      *cmdPtr = FindMethod(object->nsPtr, *methodNamePtr);
      if ((*cmdPtr != NULL) && (Tcl_Command_flags(*cmdPtr) & NSF_CMD_CALL_PRIVATE_METHOD)) {
        /*fprintf(stderr, "NEXT found private cmd %s => %p\n", *methodNamePtr, *cmdPtr);*/
        *cmdPtr = NULL;
      }
    } else {
      *cmdPtr = NULL;
    }
  } else {
    *cmdPtr = NULL;
  }

  /*fprintf(stderr, "NEXT methodName %s *clPtr %p %s *cmd %p cscPtr->flags %.6x\n",
   *methodNamePtr, *clPtr, ClassName((*clPtr)), *cmdPtr, cscPtr->flags); */

  if (*cmdPtr == NULL) {
    NsfClasses *pl = PrecedenceOrder(object->cl);
    NsfClass *cl = *clPtr;

    if (cl != NULL) {
      /*
       * Skip until actual class
       */
      for ( ; pl; pl = pl->nextPtr) {
        if (pl->cl == cl) {
          pl = pl->nextPtr;
          break;
        }
      }
    }

    if (pl != NULL) {
      /*
       * Search for a further class method. When we are called from an active
       * filter and the call had the -local flag set, then allow to call private methods.
       */
      *clPtr = SearchPLMethod(pl, *methodNamePtr, cmdPtr,
                              ((cscPtr->flags & NSF_CM_LOCAL_METHOD) != 0u &&
                               (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER) != 0u)
                              ? 0 : NSF_CMD_CALL_PRIVATE_METHOD);
    } else {
    *clPtr = NULL;
    }

  } else {
    *clPtr = NULL;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NextGetArguments --
 *
 *    Obtain arguments for a method invoked via next either from the
 *    argument vector or from the stack (call stack content or Tcl
 *    stack). In case of ensemble calls the stack entries of the
 *    ensemble invocation are used. The function returns the arguments
 *    4 to 8.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int NextGetArguments(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                 NsfCallStackContent **cscPtrPtr, const char **methodNamePtr,
                 int *outObjc, Tcl_Obj ***outObjv, int *freeArgumentVector)
  nonnull(1) nonnull(4) nonnull(5) nonnull(6) nonnull(7) nonnull(8);

static int
NextGetArguments(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                 NsfCallStackContent **cscPtrPtr, const char **methodNamePtr,
                 int *outObjc, Tcl_Obj ***outObjv, int *freeArgumentVector) {
  Tcl_Obj **nobjv;
  int nobjc, oc, inEnsemble;
  Tcl_CallFrame *framePtr;
  NsfCallStackContent *cscPtr = CallStackGetTopFrame(interp, &framePtr);

  nonnull_assert(interp != NULL);
  nonnull_assert(cscPtrPtr != NULL);
  nonnull_assert(methodNamePtr != NULL);
  nonnull_assert(outObjc != NULL);
  nonnull_assert(outObjv != NULL);
  assert(freeArgumentVector != NULL);

  /* always make sure, we only decrement when necessary */
  *freeArgumentVector = 0;

  if (cscPtr == NULL) {
    return NsfPrintError(interp, "next: can't find self");
  }

  if (cscPtr->cmdPtr == NULL) {
    return NsfPrintError(interp, "next: no executing proc");
  }

  oc = Tcl_CallFrame_objc(framePtr);

  if ((cscPtr->frameType & NSF_CSC_TYPE_ENSEMBLE)) {
    /*
     * We are in an ensemble method. The next works here not on the
     * actual methodName + frame, but on the ensemble above it. We
     * locate the appropriate call-stack content and continue next on
     * that.
     */
    cscPtr = CallStackFindEnsembleCsc(framePtr, &framePtr);
    assert(cscPtr != NULL);
    inEnsemble = 1;
    *methodNamePtr = ObjStr(cscPtr->objv[0]);
  } else {
    inEnsemble = 0;
    *methodNamePtr = Tcl_GetCommandName(interp, cscPtr->cmdPtr);
  }

  /*fprintf(stderr, "NextGetArguments oc %d objc %d inEnsemble %d objv %p\n",
    oc, objc, inEnsemble, cscPtr->objv); */

  if (objc > -1) {
    int methodNameLength;
    /*
     * Arguments were provided. We have to construct an argument
     * vector with the first argument(s) as the method name. In an
     * ensemble, we have to insert the objs of the full ensemble name.
     */
    if (inEnsemble != 0) {
      methodNameLength = 1 + cscPtr->objc - oc;
      nobjc = objc + methodNameLength;
      nobjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * nobjc);
      MEM_COUNT_ALLOC("nextArgumentVector", nobjv);
      /*
       * Copy the ensemble path name
       */
      memcpy((char *)nobjv, cscPtr->objv, sizeof(Tcl_Obj *) * methodNameLength);

     } else {
      methodNameLength = 1;
      nobjc = objc + methodNameLength;
      nobjv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj *) * nobjc);
      MEM_COUNT_ALLOC("nextArgumentVector", nobjv);
      /*
       * Copy the method name
       */
      if (cscPtr->objv != NULL) {
        nobjv[0] = cscPtr->objv[0];
      } else if (Tcl_CallFrame_objv(framePtr)) {
        nobjv[0] = Tcl_CallFrame_objv(framePtr)[0];
      }
    }

    if (objc > 0) {
      /*
       * Copy the remaining argument vector
       */
      assert((objv != NULL) || (cscPtr->objv != NULL));
      memcpy(nobjv + methodNameLength, objv == NULL ? cscPtr->objv : objv, sizeof(Tcl_Obj *) * objc);
    }

    INCR_REF_COUNT(nobjv[0]); /* we seem to need this here */
    *freeArgumentVector = 1;
  } else {
    /*
     * No arguments were provided
     */
    if (cscPtr->objv != NULL) {
      nobjv = (Tcl_Obj **)cscPtr->objv;
      nobjc = cscPtr->objc;
    } else {
      nobjc = Tcl_CallFrame_objc(framePtr);
      nobjv = (Tcl_Obj **)Tcl_CallFrame_objv(framePtr);
    }
  }

  *cscPtrPtr = cscPtr;
  *outObjc = nobjc;
  *outObjv = nobjv;

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NextInvokeFinalize --
 *
 *    This finalize function is either called via NRE callback or
 *    directly (from NextSearchAndInvoke). It resets after a successul
 *    lookup and invocation the continuation context (filter flags etc)
 *    and cleans up optionally the argument vector (inverse operation
 *    of NextGetArguments).
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    freeing memory
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int NextInvokeFinalize(ClientData data[], Tcl_Interp *interp, int result)
  nonnull(1) nonnull(2);

NSF_INLINE static int
NextInvokeFinalize(ClientData data[], Tcl_Interp *interp, int result) {
  Tcl_Obj **nobjv = data[0];
  NsfCallStackContent *cscPtr = data[1];

  nonnull_assert(data != NULL);
  nonnull_assert(interp != NULL);

  /*fprintf(stderr, "***** NextInvokeFinalize cscPtr %p flags %.6x is next %d result %d unk %d\n",
          cscPtr, cscPtr->flags, cscPtr->flags & NSF_CSC_CALL_IS_NEXT, result,
          RUNTIME_STATE(interp)->unknown);*/

  if ((cscPtr->flags & NSF_CSC_CALL_IS_NEXT) != 0u) {
    /* fprintf(stderr, "..... it was a successful next\n"); */
    cscPtr->flags &= ~NSF_CSC_CALL_IS_NEXT;

    if (cscPtr->frameType == NSF_CSC_TYPE_INACTIVE_FILTER) {
      cscPtr->frameType = NSF_CSC_TYPE_ACTIVE_FILTER;
    } else if (cscPtr->frameType == NSF_CSC_TYPE_INACTIVE_MIXIN) {
      cscPtr->frameType = NSF_CSC_TYPE_ACTIVE_MIXIN;
    }
  }

  if (nobjv != NULL) {
    DECR_REF_COUNT(nobjv[0]);
    MEM_COUNT_FREE("nextArgumentVector", nobjv);
    ckfree((char *)nobjv);
  }

  if (result == TCL_ERROR && RUNTIME_STATE(interp)->unknown) {
    /* fprintf(stderr, "don't report unknown error\n"); */
    /*
     * Don't report "unknown" errors via next.
     */
    result = TCL_OK;
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * NextSearchAndInvoke --
 *
 *    The function is called with a final argument vector and searches for a
 *    possibly shadowed method. If a target method is found, this dispatcher
 *    function updates the continuation context (filter flags etc.), invokes
 *    upon the target method, and performs a cleanup.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The invoked method might produce side effects. Also, the interp's unknown
 *    state may be modified.
 *
 *----------------------------------------------------------------------
 */
static int
NextSearchAndInvoke(Tcl_Interp *interp, const char *methodName,
                    int objc, Tcl_Obj *CONST objv[],
                    NsfCallStackContent *cscPtr,
                    int freeArgumentVector) {
  Tcl_Command cmd = NULL, currentCmd = NULL;
  int result, isMixinEntry = 0, isFilterEntry = 0,
    endOfFilterChain = 0;
  NsfRuntimeState *rst = RUNTIME_STATE(interp);
  NsfObject *object;
  NsfClass *cl;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(cscPtr != NULL);

  /*
   * Search the next method & compute its method data
   */
  cl = cscPtr->cl;
  object = cscPtr->self;
  result = NextSearchMethod(object, interp, cscPtr, &cl, &methodName, &cmd,
                            &isMixinEntry, &isFilterEntry, &endOfFilterChain, &currentCmd);

  /*fprintf(stderr, "NEXT search on %s.%s cl %p cmd %p endOfFilterChain %d result %d IS OK %d\n",
    ObjectName(object), methodName, cl, cmd, endOfFilterChain, result, (result == TCL_OK));*/

  if (unlikely(result != TCL_OK)) {
    goto next_search_and_invoke_cleanup;
  }

#if 0
  Tcl_ResetResult(interp); /* needed for bytecode support */
#endif
  if (cmd != NULL) {
    int frameType = NSF_CSC_TYPE_PLAIN;
    /*
     * change mixin state
     */
    if (object->mixinStack != NULL) {
      if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_MIXIN) {
        cscPtr->frameType = NSF_CSC_TYPE_INACTIVE_MIXIN;
      }
      /* otherwise move the command pointer forward */
      if (isMixinEntry != 0) {
        frameType = NSF_CSC_TYPE_ACTIVE_MIXIN;
        object->mixinStack->currentCmdPtr = currentCmd;
      }
    }
    /*
     * change filter state
     */
    if (object->filterStack != NULL) {
      if (cscPtr->frameType == NSF_CSC_TYPE_ACTIVE_FILTER) {
        /*fprintf(stderr, "next changes filter state\n");*/
        cscPtr->frameType = NSF_CSC_TYPE_INACTIVE_FILTER;
      }

      /* otherwise move the command pointer forward */
      if (isFilterEntry != 0) {
        /*fprintf(stderr, "next moves filter forward\n");*/
        frameType = NSF_CSC_TYPE_ACTIVE_FILTER;
        object->filterStack->currentCmdPtr = currentCmd;
      }
    }

    /*
     * now actually call the "next" method
     */

    cscPtr->flags |= NSF_CSC_CALL_IS_NEXT;
    rst->unknown = 0;
#if defined(NRE)
    { unsigned int flags;
      /*
       * Allow call only without immediate flag, when caller has NRE without immediate
       */
      flags = NsfImmediateFromCallerFlags(cscPtr->flags);

      /*fprintf(stderr, "MethodDispatch in next flags %.6x NRE %d immediate %d next-flags %.6x\n",
        cscPtr->flags,
        (cscPtr->flags & NSF_CSC_CALL_IS_NRE) != 0,
        (cscPtr->flags & NSF_CSC_IMMEDIATE) != 0,
        flags);*/

      if (flags == 0) {
        /*
         * The call is NRE-enabled. We register the callback and return
         * here immediately.  All other exists form this functions have
         * to call NextInvokeFinalize manually on return.
         */
        Tcl_NRAddCallback(interp, NextInvokeFinalize,
                          (freeArgumentVector != 0) ? (ClientData)objv : NULL,
                          cscPtr, NULL, NULL);
        return MethodDispatch(object, interp, objc, objv, cmd,
                              object, cl, methodName, frameType, flags);
      } else {
        result = MethodDispatch(object, interp, objc, objv, cmd,
                                object, cl, methodName, frameType, flags);
      }
    }
#else
    /*fprintf(stderr, "NextSearchAndWinvoke calls cmd %p methodName %s cscPtr->flags %.8x\n",
      cmd, methodName, cscPtr->flags);*/
    result = MethodDispatch(object, interp, objc, objv, cmd,
                            object, cl, methodName, frameType, cscPtr->flags);
#endif
  } else if (likely(result == TCL_OK)) {
    NsfCallStackContent *topCscPtr;
    Tcl_CallFrame *varFramePtr = NULL;
    int isLeafNext;

    /*
     * We could not find a cmd, yet the dispatch attempt did not result
     * in an error. This means that we find ourselves in either of three
     * situations at this point:
     *
     * 1) An explicit "next" cmd (NsfNextCmd()) at the end of a filter chain:
     * Dispatch to unknown as there is no implementation for the requested
     * call available.
     *
     * 2) An explicit "next" cmd from within a leaf sub-method (a "leaf
     * next"): Remain silent, do not dispatch to unknown.

     * 3) An implicit "next" triggered for unresolved submethods that might be
     * resolved along the next path: Dispatch to unknown, the requested
     * sub-cmd is not resolvable to a cmd.
     *
     * For the cases 1) and 3), set the interp's unknown flag signaling to
     * higher levels (e.g., in MethodDispatchCsc(), in NsfNextCmd()) the need
     * for dispatching to unknown.
     */

    /* NsfShowStack(interp);*/

    topCscPtr = CallStackGetTopFrame(interp, &varFramePtr);
    assert(topCscPtr != NULL);
    assert(varFramePtr != NULL);

    /*
     * Find the appropriate frame pointing to the start of the ensemble, in
     * case we are in the middle of an ensemble.
     */
    /*fprintf(stderr, "######## cscPtr %p topCscPtr %p\n", cscPtr, topCscPtr);*/
    if ( cscPtr != topCscPtr
         && (cscPtr->flags & NSF_CSC_CALL_IS_ENSEMBLE) != 0u
         && (topCscPtr->flags & NSF_CSC_CALL_IS_ENSEMBLE) != 0u) {

      for (; varFramePtr != NULL; varFramePtr = Tcl_CallFrame_callerPtr(varFramePtr)) {
        topCscPtr = (NsfCallStackContent *)Tcl_CallFrame_clientData(varFramePtr);
        assert(topCscPtr != NULL);
        /*fprintf(stderr, "######## cscPtr %p topCscPtr %p topCscPtr->flags %8x\n",
          cscPtr, topCscPtr, (topCscPtr != NULL) ? topCscPtr->flags : 0);*/
        if ((topCscPtr->flags & NSF_CM_ENSEMBLE_UNKNOWN) != 0u) {
          break;
        }
      }

      if (varFramePtr != NULL) {
        varFramePtr = Tcl_CallFrame_callerPtr(varFramePtr);
        if (Tcl_CallFrame_isProcCallFrame(varFramePtr) & (FRAME_IS_NSF_METHOD|FRAME_IS_NSF_CMETHOD)) {
          topCscPtr = (NsfCallStackContent *)Tcl_CallFrame_clientData(varFramePtr);
          assert(topCscPtr != NULL);
        }
      }
    }

    /* case 2 */
    isLeafNext = (cscPtr != topCscPtr)
      && (topCscPtr->frameType & NSF_CSC_TYPE_ENSEMBLE) != 0u
      && (topCscPtr->flags & NSF_CSC_CALL_IS_ENSEMBLE) == 0u;

    rst->unknown = /* case 1 */ endOfFilterChain ||
      /* case 3 */ (!isLeafNext && ((cscPtr->flags & NSF_CSC_CALL_IS_ENSEMBLE) != 0u));

    /*fprintf(stderr, "******** setting unknown to %d isLeafNext %d topCscPtr %p endOfFilterChain %d\n",
      rst->unknown, isLeafNext, topCscPtr, endOfFilterChain);*/
  }

 next_search_and_invoke_cleanup:
  /*
   * We come here, whenever the NRE callback is NOT registered
   */
  {ClientData data[2] = {
      freeArgumentVector ? (ClientData)objv : NULL,
      cscPtr
    };
    return NextInvokeFinalize(data, interp, result);
  }
}

/*
 *----------------------------------------------------------------------
 * NsfNextObjCmd --
 *
 *    nsf::xotclnext is for backwards compatibility to the next
 *    implementation in XOTcl.  It receives an argument vector which
 *    is used for the invocation. if no argument vector is provided,
 *    the argument vector of the last invocation is used. If the
 *    argument vector starts with "--noArgs", then no arguments are
 *    passed to the shadowed method.
 *
 *    TODO: On the longer range, this function should go into an external
 *    library (e.g. XOTcl compatibility library)
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The invoked method might produce side effects
 *
 *----------------------------------------------------------------------
 */
static int
NsfNextObjCmd(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(2) nonnull(4);

static int
NsfNextObjCmd(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  int freeArgumentVector, result, nobjc = 0;
  const char *methodName = NULL;
  NsfCallStackContent *cscPtr;
  Tcl_Obj **nobjv;

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  if (likely(objc < 2)) {
    /* No arguments were provided */
    objc = 0;
  } else {
    /* in case --noArgs is used, remove the flag and provide an empty argument list */
    const char *arg1String = ObjStr(objv[1]);

    if (*arg1String == '-' && !strcmp(arg1String, "--noArgs")) {
      objc = 1;
    }
  }

  result = NextGetArguments(interp, objc-1, &objv[1], &cscPtr, &methodName,
                            &nobjc, &nobjv, &freeArgumentVector);
  if (likely(result == TCL_OK)) {
    assert(nobjc > 0);
    result = NextSearchAndInvoke(interp, methodName, nobjc, nobjv, cscPtr, freeArgumentVector);
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * FindSelfNext --
 *
 *    This function is called via [current nextmethod] to set the result of the
 *    interp to the method which would be called by [next]. If there are more
 *    shadowed methods along the precedence path, it sets the result of the
 *    next method in form of a method handle. If there are no more shadowed
 *    procs, the result is set to empty.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    Set Tcl result.
 *
 *----------------------------------------------------------------------
 */

static int FindSelfNext(Tcl_Interp *interp) nonnull(1);

static int
FindSelfNext(Tcl_Interp *interp) {
  NsfCallStackContent *cscPtr = CallStackGetTopFrame0(interp);
  Tcl_Command cmd = NULL, currentCmd = NULL;
  int result, isMixinEntry = 0,
    isFilterEntry = 0,
    endOfFilterChain = 0;
  NsfClass *cl = cscPtr->cl;
  NsfObject *object = cscPtr->self;
  const char *methodName;

  nonnull_assert(interp != NULL);

  Tcl_ResetResult(interp);

  methodName = Tcl_GetCommandName(interp, cscPtr->cmdPtr);
  if (methodName == NULL) {
    return TCL_OK;
  }

  result = NextSearchMethod(object, interp, cscPtr, &cl, &methodName, &cmd,
                   &isMixinEntry, &isFilterEntry, &endOfFilterChain, &currentCmd);
  if (cmd != NULL) {
    Tcl_SetObjResult(interp, MethodHandleObj((cl != NULL) ? (NsfObject *)cl : object,
                                             cl == NULL, methodName));
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 * ComputeLevelObj --
 *
 *    This function comptes a fresh Tcl_Obj referring to the interp level. The
 *    caller has to care about freeing the returned Tcl_Obj.
 *
 * Results:
 *    Tcl_Obj *
 *
 * Side effects:
 *    Allocates a new Tcl_Obj
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj * ComputeLevelObj(Tcl_Interp *interp, CallStackLevel level) nonnull(1) returns_nonnull;

static Tcl_Obj *
ComputeLevelObj(Tcl_Interp *interp, CallStackLevel level) {
  Tcl_CallFrame *framePtr;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);

  switch (level) {
  case CALLING_LEVEL: NsfCallStackFindLastInvocation(interp, 1, &framePtr); break;
  case ACTIVE_LEVEL:  NsfCallStackFindActiveFrame(interp,    1, &framePtr); break;
  default: framePtr = NULL; /* silence compiler */
  }

  if (framePtr != NULL) {
    /* the call was from an nsf frame, return absolute frame number */
    char buffer[LONG_AS_STRING];
    int l;

    buffer[0] = '#';
    Nsf_ltoa(buffer+1, (long)Tcl_CallFrame_level(framePtr), &l);
    resultObj = Tcl_NewStringObj(buffer, l+1);
  } else {
    /* If not called from an nsf frame, return 1 as default */
    resultObj = Tcl_NewIntObj(1);
  }

  return resultObj;
}

/*
  int
  NsfKObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  if (objc < 2) {
    return NsfPrintError(interp, "wrong # of args for K");
  }

  Tcl_SetObjResult(interp, objv[1]);
  return TCL_OK;
  }
*/

/*
 * object creation & destruction
 */

/*
 *----------------------------------------------------------------------
 * UnsetInAllNamespaces --
 *
 *    Try to unset a variable, searching for the variable in all
 *    name-spaces. This function is used by volatile to unset the automatic
 *    variable used for the destroy trace.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    Might unset variable
 *
 *----------------------------------------------------------------------
 */

static int UnsetInAllNamespaces(Tcl_Interp *interp, Tcl_Namespace *nsPtr, const char *name)
  nonnull(1) nonnull(2) nonnull(3);

static int
UnsetInAllNamespaces(Tcl_Interp *interp, Tcl_Namespace *nsPtr, const char *name) {
  int rc = 0;
  Tcl_HashSearch search;
  Tcl_HashEntry *entryPtr;
  Tcl_Var *varPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);
  nonnull_assert(name != NULL);

  /*fprintf(stderr, "### UnsetInAllNamespaces variable '%s', current namespace '%s'\n",
          name, (nsPtr != NULL) ? nsPtr->fullName : "NULL");*/

  entryPtr = Tcl_FirstHashEntry(Tcl_Namespace_childTablePtr(nsPtr), &search);
  varPtr = (Tcl_Var *) Tcl_FindNamespaceVar(interp, name, nsPtr, 0);
  /*fprintf(stderr, "found %s in %s -> %p\n", name, nsPtr->fullName, varPtr);*/
  if (varPtr != NULL) {
    Tcl_DString dFullname, *dsPtr = &dFullname;
    int result;

    Tcl_DStringInit(dsPtr);
    Tcl_DStringAppend(dsPtr, "unset ", -1);
    DStringAppendQualName(dsPtr, nsPtr, name);
    /*rc = Tcl_UnsetVar2(interp, Tcl_DStringValue(dsPtr), NULL, TCL_LEAVE_ERR_MSG);*/
    result = Tcl_Eval(interp, Tcl_DStringValue(dsPtr));
    /* fprintf(stderr, "fqName = '%s' unset => %d %d\n", Tcl_DStringValue(dsPtr), rc, TCL_OK);*/
    if (likely(result == TCL_OK)) {
      rc = 1;
    } else {
      Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
      fprintf(stderr, "   err = '%s'\n", ObjStr(resultObj));
    }
    Tcl_DStringFree(dsPtr);
  }

  while (rc == 0 && entryPtr) {
    Tcl_Namespace *childNsPtr = (Tcl_Namespace *) Tcl_GetHashValue(entryPtr);
    /*fprintf(stderr, "child = %s\n", childNsPtr->fullName);*/
    entryPtr = Tcl_NextHashEntry(&search);
    rc |= UnsetInAllNamespaces(interp, childNsPtr, name);
  }

  return rc;
}

/*
 *----------------------------------------------------------------------
 * FreeUnsetTraceVariable --
 *
 *    Unset trace variable.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    Might unset variable
 *
 *----------------------------------------------------------------------
 */

static int FreeUnsetTraceVariable(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static int
FreeUnsetTraceVariable(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->opt != NULL && object->opt->volatileVarName) {
    int result = Tcl_UnsetVar2(interp, object->opt->volatileVarName, NULL, 0);

    /*
     * Somebody destroys a volatile object manually while the vartrace is
     * still active. Destroying the object will be a problem in case the
     * variable is deleted later and fires the trace. So, we unset the
     * variable here which will cause a destroy via var trace, which in turn
     * clears the volatileVarName flag.
     */
    /* fprintf(stderr, "### FreeUnsetTraceVariable %s\n", object->opt->volatileVarName);*/

    if (unlikely(result != TCL_OK)) {
      int result = Tcl_UnsetVar2(interp, object->opt->volatileVarName, NULL, TCL_GLOBAL_ONLY);
      if (unlikely(result != TCL_OK)) {
        Tcl_Namespace *nsPtr = Tcl_GetCurrentNamespace(interp);
        if (UnsetInAllNamespaces(interp, nsPtr, object->opt->volatileVarName) == 0) {
          fprintf(stderr, "### don't know how to delete variable '%s' of volatile object\n",
                  object->opt->volatileVarName);
          /*
           * Return always success, since an error during destroy does not
           * help at all
           */
        }
      }
    }
    /*fprintf(stderr, "### FreeUnsetTraceVariable returns %d OK %d\n", result, TCL_OK);*/
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NsfUnsetTrace --
 *
 *    Function to be triggered whenever the trigger variable is
 *    deleted. Typically this function deletes the associated object.
 *
 * Results:
 *    Result msg or null
 *
 * Side effects:
 *    Might delete associated object
 *
 *----------------------------------------------------------------------
 */

static const char *NsfUnsetTrace(ClientData clientData, Tcl_Interp *interp,
                           const char *UNUSED(name), const char *UNUSED(name2), unsigned int flags)
  nonnull(1) nonnull(2);

static const char *
NsfUnsetTrace(ClientData clientData, Tcl_Interp *interp,
              const char *UNUSED(name), const char *UNUSED(name2), unsigned int flags)
{
  Tcl_Obj *objPtr = (Tcl_Obj *)clientData;
  NsfObject *object;
  const char *resultMsg = NULL;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);

  /*fprintf(stderr, "NsfUnsetTrace %s flags %.4x %.4x\n", name, flags,
    flags & TCL_INTERP_DESTROYED);*/

  if ((flags & TCL_INTERP_DESTROYED) == 0u) {
    if (GetObjectFromObj(interp, objPtr, &object) == TCL_OK) {
      Tcl_Obj *savedResultObj = Tcl_GetObjResult(interp); /* save the result */

      INCR_REF_COUNT(savedResultObj);

      /* clear variable, destroy is called from trace */
      if (object->opt != NULL && object->opt->volatileVarName) {
        object->opt->volatileVarName = NULL;
      }

      if (DispatchDestroyMethod(interp, object, 0) != TCL_OK) {
        resultMsg = "Destroy for volatile object failed";
      } else {
        resultMsg = "No nsf Object passed";
      }

      Tcl_SetObjResult(interp, savedResultObj);  /* restore the result */
      DECR_REF_COUNT(savedResultObj);
    }
    DECR_REF_COUNT(objPtr);
  } else {
    /*fprintf(stderr, "omitting destroy on %s %p\n", name);*/
  }
  return resultMsg;
}

/*
 *----------------------------------------------------------------------
 * CleanupDestroyObject --
 *
 *    Perform cleanup of object; after the function is executed, the object is
 *    in the same fresh state as after initialization.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Possibly freeing memory.
 *
 *----------------------------------------------------------------------
 */
static void CleanupDestroyObject(Tcl_Interp *interp, NsfObject *object, int softrecreate) nonnull(1) nonnull(2);

static void
CleanupDestroyObject(Tcl_Interp *interp, NsfObject *object, int softrecreate) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*fprintf(stderr, "CleanupDestroyObject obj %p softrecreate %d nsPtr %p\n",
    object, softrecreate, object->nsPtr);*/

  /*
   * The object pointer is guaranteed to point to the same object, so it is
   * not sufficient for methodObj validation. Therefore, for objects
   * containing per-object methods, we increment the objectMethodEpoch.
   */
  if (object->nsPtr != NULL) {
    NsfObjectMethodEpochIncr("CleanupDestroyObject");
  }

  /*
   * Remove the instance, but not for ::Class/::Object
   */
  if (IsBaseClass(object) == 0) {

    if (softrecreate == 0) {
      RemoveInstance(object, object->cl);
    }
  }

  if (object->nsPtr != NULL) {
    NSCleanupNamespace(interp, object->nsPtr);
    NSDeleteChildren(interp, object->nsPtr);
  }

  if (object->varTablePtr != NULL) {
    TclDeleteVars(((Interp *)interp), object->varTablePtr);

    ckfree((char *)object->varTablePtr);
    /*FREE(obj->varTablePtr, obj->varTablePtr);*/
    object->varTablePtr = 0;
  }

  if (object->opt != NULL) {
    NsfObjectOpt *opt = object->opt;
#if defined(NSF_WITH_ASSERTIONS)
    if (opt->assertions != NULL) {
      AssertionRemoveStore(opt->assertions);
      opt->assertions = NULL;
    }
#endif

#if defined(PER_OBJECT_PARAMETER_CACHING)
    if (object->opt->parsedParamPtr != NULL) {
      NsfParameterCacheObjectInvalidateCmd(interp, object);
    }
#endif

    if (softrecreate == 0) {
      /*
       * Remove this object from all per object mixin lists and clear the
       * mixin list.
       */
      if (opt->objMixins != NULL) {
        RemoveFromObjectMixinsOf(object->id, opt->objMixins);
      }

      CmdListFree(&opt->objMixins, GuardDel);
      CmdListFree(&opt->objFilters, GuardDel);
      FREE(NsfObjectOpt, opt);
      object->opt = NULL;
    }
  }

  object->flags &= ~NSF_MIXIN_ORDER_VALID;
  if (object->mixinOrder != NULL) {
    MixinResetOrder(object);
  }
  object->flags &= ~NSF_FILTER_ORDER_VALID;
  if (object->filterOrder != NULL) {
    FilterResetOrder(object);
  }
}

/*
 * obj initialization & namespace creation
 */

/*
 *----------------------------------------------------------------------
 * CleanupInitObject --
 *
 *    Perform the initialization of an object in a virgin state.
 *    During bootstrap, cl might be NULL.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Updateing the object structure
 *
 *----------------------------------------------------------------------
 */
static void CleanupInitObject(Tcl_Interp *interp, NsfObject *object,
                              NsfClass *cl, Tcl_Namespace *nsPtr, int softrecreate)
  nonnull(1) nonnull(2);

static void
CleanupInitObject(Tcl_Interp *interp, NsfObject *object,
                  NsfClass *cl, Tcl_Namespace *nsPtr, int softrecreate) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "+++ CleanupInitObject\n");
#endif
  object->teardown = interp;
  object->nsPtr = nsPtr;

  if (!softrecreate && cl != NULL) {
    AddInstance(object, cl);
  }
  if ((object->flags & NSF_RECREATE) != 0u) {
    object->opt = NULL;
    object->varTablePtr = NULL;
    object->mixinOrder = NULL;
    object->filterOrder = NULL;
    object->flags = 0;
  }
  /*
    fprintf(stderr, "cleanupInitObject %s: %p cl = %p\n", (obj->cmdName != NULL) ? ObjectName(object) : "", object, object->cl);*/
}

/*
 *----------------------------------------------------------------------
 * PrimitiveDestroy --
 *
 *    Dispatch either PrimitiveCDestroy or PrimitiveODestroy
 *    depending on whether the object is a class
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void
PrimitiveDestroy(ClientData clientData) {

  nonnull_assert(clientData != NULL);

  if (NsfObjectIsClass((NsfObject *)clientData)) {
    PrimitiveCDestroy(clientData);
  } else {
    PrimitiveODestroy(clientData);
  }
}

/*
 *----------------------------------------------------------------------
 * TclDeletesObject --
 *
 *    Function to be called, when Tcl deletes the command which has an
 *    object/class associated. This happens, when e.g. a namespace is deleted.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void TclDeletesObject(ClientData clientData) nonnull(1);

static void
TclDeletesObject(ClientData clientData) {
  NsfObject *object;
  Tcl_Interp *interp;

  nonnull_assert(clientData != NULL);

  object = (NsfObject *)clientData;
  /*
   * TODO: Actually, it seems like a good idea to flag a deletion from Tcl by
   * setting object->id to NULL. However, we seem to have some dependencies
   * avoiding this currently, so we use the flag.
   */
  object->flags |= NSF_TCL_DELETE;

  /*fprintf(stderr, "cmd dealloc %p TclDeletesObject (%d)\n",
    object->id,  Tcl_Command_refCount(object->id));*/

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "TclDeletesObject %p obj->id %p flags %.6x\n", object, object->id, object->flags);
#endif
  if ((object->flags & NSF_DURING_DELETE) != 0u || !object->teardown) {
    return;
  }
  interp = object->teardown;
# ifdef OBJDELETION_TRACE
  fprintf(stderr, "... %p %s\n", object, ObjectName(object));
# endif

  CallStackDestroyObject(interp, object);
}


/*
 *----------------------------------------------------------------------
 * PrimitiveODestroy --
 *
 *    Delete an object with its namespace and associated data structures
 *    (mixin stack, filter stack). The physical deallocation is handled by
 *    NsfCleanupObject() which performs reference counting.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free obejct contents.
 *
 *----------------------------------------------------------------------
 */
static void
PrimitiveODestroy(ClientData clientData) {
  NsfObject *object;
  Tcl_Interp *interp;

  nonnull_assert(clientData != NULL);

  object = (NsfObject *)clientData;
  assert(object->teardown != NULL);

  /*fprintf(stderr, "****** PrimitiveODestroy %p cmd %p flags %.6x\n",
    (void *)object, (void *)object->id, object->flags);*/

  /*
   * We assume, the object was not yet deleted, but destroy was called
   * already.
   */
  assert((object->flags & NSF_DELETED) == 0u);
  assert((object->flags & NSF_DESTROY_CALLED) != 0u);

  /*
   * Check and latch against recurrent calls with object->teardown.
   */
  PRINTOBJ("PrimitiveODestroy", object);
  interp = object->teardown;

  /*
   * Don't destroy, if the interpreter is destroyed already
   * e.g. TK calls Tcl_DeleteInterp directly, if the window is killed
   */
  if (Tcl_InterpDeleted(interp)) {
    return;
  }

#ifdef OBJDELETION_TRACE
  {Command *cmdPtr = object->id;
  fprintf(stderr, "  physical delete of %p id=%p (cmd->refCount %d) destroyCalled=%d '%s'\n",
          object, object->id, cmdPtr->refCount, (object->flags & NSF_DESTROY_CALLED), ObjectName(object));
  }
#endif
  CleanupDestroyObject(interp, object, 0);

  while (object->mixinStack) {
    MixinStackPop(object);
  }

  while (object->filterStack) {
    FilterStackPop(object);
  }

  /*
   * Object is now mostly dead, but still allocated. However, since
   * Nsf_DeleteNamespace might delegate to the parent (e.g. slots) we clear
   * teardown after the deletion of the children.
   */
  if (object->nsPtr != NULL) {
    /*fprintf(stderr, "PrimitiveODestroy calls deleteNamespace for object %p nsPtr %p\n", (void*)object, object->nsPtr);*/
    Nsf_DeleteNamespace(interp, object->nsPtr);
    object->nsPtr = NULL;
  }
  object->teardown = NULL;

  /*fprintf(stderr, " +++ OBJ/CLS free: %p %s\n", (void *)object, ObjectName(object));*/

  object->flags |= NSF_DELETED;
  ObjTrace("ODestroy", object);

  DECR_REF_COUNT(object->cmdName);
  NsfCleanupObject(object, "PrimitiveODestroy");
}

/*
 *----------------------------------------------------------------------
 * DoDealloc --
 *
 *    Perform deallocation of an object/class. This function is called
 *    from the dealloc method and internally to get rid of an
 *    abject. It cares about volatile and frees/triggers free
 *    operation depending on the stack references.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    freed object or object is marked to be freed.
 *
 *----------------------------------------------------------------------
 */
static int DoDealloc(Tcl_Interp *interp, NsfObject *object) nonnull(1) nonnull(2);

static int
DoDealloc(Tcl_Interp *interp, NsfObject *object) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*fprintf(stderr, "DoDealloc obj= %s %p flags %.6x activation %d cmd %p opt=%p\n",
          ObjectName(object), object, object->flags, object->activationCount,
          object->id, object->opt);*/

  result = FreeUnsetTraceVariable(interp, object);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  /*
   * Latch, and call delete command if not already in progress.
   */
  if (RUNTIME_STATE(interp)->exitHandlerDestroyRound !=
      NSF_EXITHANDLER_ON_SOFT_DESTROY) {
    CallStackDestroyObject(interp, object);
  }

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * MarkUndestroyed --
 *
 *    Mark an object as if destroy was not called. This function is e.g. used
 *    from recreate.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Setting object flag.
 *
 *----------------------------------------------------------------------
 */
static void MarkUndestroyed(NsfObject *object) nonnull(1);

static void
MarkUndestroyed(NsfObject *object) {

  nonnull_assert(object != NULL);

  object->flags &= ~NSF_DESTROY_CALLED;
}

/*
 *----------------------------------------------------------------------
 * PrimitiveOInit --
 *
 *    Set/reset the object to a fresh, un-destroyed state
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    initializing object structure
 *
 *----------------------------------------------------------------------
 */
static void PrimitiveOInit(NsfObject *object, Tcl_Interp *interp, const char *name,
                           Tcl_Namespace *nsPtr, NsfClass *cl)
  nonnull(1) nonnull(2) nonnull(3);

static void
PrimitiveOInit(NsfObject *object, Tcl_Interp *interp, const char *name,
               Tcl_Namespace *nsPtr, NsfClass *cl) {

  nonnull_assert(object != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "+++ PrimitiveOInit\n");
#endif

#ifdef NSFOBJ_TRACE
  fprintf(stderr, "OINIT %s = %p\n", name, object);
#endif
  NsfObjectRefCountIncr(object);
  MarkUndestroyed(object);

  /*
   * There might be already a namespace with name name; if this is the
   * case, use this namespace as object namespace. The preexisting
   * namespace might contain Next Scripting objects. If we would not use the
   * namespace as child namespace, we would not recognize the objects
   * as child objects, deletions of the object might lead to a crash.
   *
   * We can use here the provided nsPtr, except in cases, where this
   * namespaces is being destroyed (e.g. recreate a new object from a
   * different object system).
   */

  if (nsPtr != NULL && (((Namespace *)nsPtr)->flags & NS_DYING) != 0u) {
    Namespace *dummy1Ptr, *dummy2Ptr, *nsPtr1 = (Namespace *)nsPtr;
    const char *dummy;
    TclGetNamespaceForQualName(interp, name,
                               NULL, TCL_GLOBAL_ONLY|TCL_FIND_ONLY_NS,
                               &nsPtr1, &dummy1Ptr, &dummy2Ptr, &dummy);
    nsPtr = (Tcl_Namespace *)nsPtr1;
    /*fprintf(stderr, "PrimitiveOInit %p calls TclGetNamespaceForQualName with %s => %p given %p object->nsPtr %p\n",
            object, name,
            nsPtr, nsPtr, object->nsPtr);*/
  }

  if (nsPtr != NULL) {
    NsfNamespaceInit(nsPtr);
  }

  /* fprintf(stderr, "PrimitiveOInit %p %s, ns %p\n", object, name, nsPtr); */
  CleanupInitObject(interp, object, cl, nsPtr, 0);

  /* TODO: would be nice, if we could init object flags */
  /* object->flags = NSF_MIXIN_ORDER_VALID | NSF_FILTER_ORDER_VALID;*/
  object->mixinStack = NULL;
  object->filterStack = NULL;
}

/*
 *----------------------------------------------------------------------
 * PrimitiveOCreate --
 *
 *    Allocate memory for an object, create the object name and the associated
 *    Tcl command and call the initialization functions.
 *
 * Results:
 *    NsfObject*
 *
 * Side effects:
 *    Allocating memory
 *
 *----------------------------------------------------------------------
 */
static NsfObject * PrimitiveOCreate(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Namespace *parentNsPtr, NsfClass *cl)
  nonnull(1) nonnull(2) nonnull(4) returns_nonnull;

static NsfObject *
PrimitiveOCreate(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Namespace *parentNsPtr, NsfClass *cl) {
  const char *nameString;
  Tcl_Namespace *nsPtr;
  NsfObject *object;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(cl != NULL);

  object = (NsfObject *)ckalloc(sizeof(NsfObject));
  MEM_COUNT_ALLOC("NsfObject/NsfClass", object);
  assert(object != NULL); /* ckalloc panics, if malloc fails */

  memset(object, 0, sizeof(NsfObject));

  nameString = ObjStr(nameObj);
  assert(isAbsolutePath(nameString));

#if defined(NSFOBJ_TRACE)
  fprintf(stderr, "CKALLOC Object %p %s\n", object, nameString);
#endif
#ifdef OBJDELETION_TRACE
  fprintf(stderr, "+++ PrimitiveOCreate\n");
#endif

  nsPtr = NSCheckNamespace(interp, nameString, parentNsPtr);
  if (nsPtr != NULL) {
    NSNamespacePreserve(nsPtr);
  }
#if defined(NRE)
  object->id = Tcl_NRCreateCommand(interp, nameString,
                                   NsfObjDispatch,
                                   NsfObjDispatchNRE,
                                   object, TclDeletesObject);
#else
  object->id = Tcl_CreateObjCommand(interp, nameString, NsfObjDispatch,
                                    object, TclDeletesObject);
#endif

  /*fprintf(stderr, "cmd alloc %p %d (%s)\n", object->id,
    Tcl_Command_refCount(object->id), nameString);*/

  PrimitiveOInit(object, interp, nameString, nsPtr, cl);
  if (nsPtr != NULL) {
    NSNamespaceRelease(nsPtr);
  }

  object->cmdName = nameObj;
  INCR_REF_COUNT(object->cmdName);

  ObjTrace("PrimitiveOCreate", object);

  return object;
}

/*
 *----------------------------------------------------------------------
 * DefaultSuperClass --
 *
 *    Determine the default Superclass of the class (specified as
 *    second argument) and meta class (third argument). The function
 *    searches for the variable NSF_DEFAULTMETACLASS or
 *    NSF_DEFAULTSUPERCLASS and uses it if present.
 *
 * Results:
 *    Default superclass or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static NsfClass *
DefaultSuperClass(Tcl_Interp *interp, NsfClass *cl, NsfClass *mCl, int isMeta) {
  NsfClass *resultClass = NULL;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(mCl != NULL);

  /*fprintf(stderr, "DefaultSuperClass cl %s, mcl %s, isMeta %d\n",
    ClassName(cl), ClassName(mCl), isMeta );*/

  resultObj = Nsf_ObjGetVar2((Nsf_Object *)mCl, interp, (isMeta != 0) ?
                             NsfGlobalObjs[NSF_DEFAULTMETACLASS] :
                             NsfGlobalObjs[NSF_DEFAULTSUPERCLASS], NULL, 0);

  if (resultObj != NULL) {
    if (unlikely(GetClassFromObj(interp, resultObj, &resultClass, 0) != TCL_OK)) {
      NsfPrintError(interp, "default superclass is not a class");
    }
    /*fprintf(stderr, "DefaultSuperClass for %s got from var %s => %s\n",
            ClassName(cl),
            ObjStr((isMeta != 0) ? NsfGlobalObjs[NSF_DEFAULTMETACLASS] : NsfGlobalObjs[NSF_DEFAULTSUPERCLASS]),
            ClassName(resultClass));*/
  } else {
    NsfClasses *sc;

    /*fprintf(stderr, "DefaultSuperClass for %s: search in superClasses starting with %p meta %d\n",
      ClassName(cl), cl->super, isMeta);*/

    if (isMeta != 0) {
      /*
       * Is this already the root metaclass ?
       */
      if (IsRootMetaClass(mCl->object.cl)) {
        return mCl->object.cl;
      }
    }

    /*
     * check superClasses of metaclass
     */
    for (sc = mCl->super; sc && sc->cl != cl; sc = sc->nextPtr) {
      /* fprintf(stderr, "  ... check ismeta %d %s root mcl %d root cl %d\n",
         isMeta, ClassName(sc->cl),
         sc->cl->object.flags & NSF_IS_ROOT_META_CLASS,
         sc->cl->object.flags & NSF_IS_ROOT_CLASS); */
      if (isMeta != 0) {
        if (IsRootMetaClass(sc->cl)) {
          return sc->cl;
        }
      } else {
        if (IsRootClass(sc->cl)) {
          /* fprintf(stderr, "found root class %p %s\n", sc->cl, ClassName(sc->cl)); */
          return sc->cl;
        }
      }

      resultClass = DefaultSuperClass(interp, cl, sc->cl, isMeta);
      if (resultClass != NULL) {
        break;
      }
    }
  }

  return resultClass;
}

/*
 *----------------------------------------------------------------------
 * CleanupDestroyClass --
 *
 *    Cleanup class in a destroy call.  Remove filters, mixins, assertions,
 *    instances and remove finally class from class hierarchy. In the recreate
 *    case, it preserves the pointers from other class structures.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Updated class structures.
 *
 *----------------------------------------------------------------------
 */

static void CleanupDestroyClass(Tcl_Interp *interp, NsfClass *cl, int softrecreate, int recreate)
  nonnull(1) nonnull(2);

static void
CleanupDestroyClass(Tcl_Interp *interp, NsfClass *cl, int softrecreate, int recreate) {
  NsfClassOpt *clopt;
  NsfClass *baseClass = NULL;
  NsfClasses *subClasses;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  PRINTOBJ("CleanupDestroyClass", (NsfObject *)cl);
  assert((softrecreate != 0) ? recreate == 1 : 1);

  clopt = cl->opt;
  /*fprintf(stderr, "CleanupDestroyClass %p %s (ismeta=%d) softrecreate=%d, recreate=%d, %p\n",
          cl, ClassName(cl), IsMetaClass(interp, cl, 1),
          softrecreate, recreate, clopt);*/

  subClasses = DependentSubClasses(cl);
  if (subClasses != NULL) {

    /*
     * Perform the next steps even with clopt == NULL, since the class
     * might be used as a superclass of a per object mixin, so it might
     * have no clopt...
     */
    MixinInvalidateObjOrders(interp, cl, subClasses);
    if (FiltersDefined(interp) > 0) {
      FilterInvalidateObjOrders(interp, subClasses);
    }
  }

  if (clopt != NULL) {
    /*
     *  Remove this class from all isClassMixinOf lists and clear the
     *  class mixin list
     */
    if (clopt->classMixins != NULL) {
      RemoveFromClassMixinsOf(clopt->id, clopt->classMixins);
    }

    CmdListFree(&clopt->classMixins, GuardDel);
    CmdListFree(&clopt->classFilters, GuardDel);

    if (clopt->mixinRegObjs != NULL) {
      NsfMixinregInvalidate(interp, clopt->mixinRegObjs);
      DECR_REF_COUNT2("mixinRegObjs", clopt->mixinRegObjs);
      clopt->mixinRegObjs = NULL;
    }

    if (recreate == 0) {
      /*
       *  Remove this class from all mixin lists and clear the isObjectMixinOf list
       */
      if (clopt->isObjectMixinOf != 0) RemoveFromObjectMixins(clopt->id, clopt->isObjectMixinOf);
      CmdListFree(&clopt->isObjectMixinOf, GuardDel);

      /*
       *  Remove this class from all class mixin lists and clear the
       *  isClassMixinOf list
       */
      if (clopt->isClassMixinOf != 0) RemoveFromClassmixins(clopt->id, clopt->isClassMixinOf);
      CmdListFree(&clopt->isClassMixinOf, GuardDel);
    }

    /*
     * Remove dependent filters of this class from all subclasses
     */
    if (subClasses != NULL) {
      FilterRemoveDependentFilterCmds(cl, subClasses);
    }

#if defined(NSF_WITH_ASSERTIONS)
    if (clopt->assertions != NULL) {
        AssertionRemoveStore(clopt->assertions);
        clopt->assertions = NULL;
      }
#endif

#ifdef NSF_OBJECTDATA
    NsfFreeObjectData(cl);
#endif
  }

  NSCleanupNamespace(interp, cl->nsPtr);
  NSDeleteChildren(interp, cl->nsPtr);

  if (softrecreate == 0) {

    /*
     * Reclass all instances of the current class to the appropriate
     * most general class ("baseClass"). The most general class of a
     * metaclass is the root meta class, the most general class of an
     * object is the root class. Instances of meta-classes can be only
     * reset to the root meta class (and not to to the root base
     * class).
     */
    baseClass = DefaultSuperClass(interp, cl, cl->object.cl,
                                  IsMetaClass(interp, cl, 1));
    /*
     * We do not have to reclassing in case, cl is a root class
     */
    if (IsRootClass(cl) == 0) {
      Tcl_HashTable *instanceTablePtr = &cl->instances;
      Tcl_HashSearch hSrch;
      Tcl_HashEntry *hPtr;

      for (hPtr = Tcl_FirstHashEntry(instanceTablePtr, &hSrch); hPtr;
           hPtr = Tcl_NextHashEntry(&hSrch)) {
        NsfObject *inst = (NsfObject *)Tcl_GetHashKey(instanceTablePtr, hPtr);
        /*fprintf(stderr, "    inst %p %s flags %.6x id %p baseClass %p %s\n",
          inst, ObjectName(inst), inst->flags, inst->id, baseClass, ClassName(baseClass));*/
        if (inst && inst != (NsfObject *)cl && ((inst->flags & NSF_DURING_DELETE) == 0u) /*inst->id*/) {
          if (inst != &(baseClass->object)) {
            AddInstance(inst, baseClass);
          }
        }
      }
    }
    Tcl_DeleteHashTable(&cl->instances);
    MEM_COUNT_FREE("Tcl_InitHashTable", &cl->instances);
  }

  if (clopt != NULL && recreate == 0) {
    FREE(NsfClassOpt, clopt);
    cl->opt = NULL;
  }

  if (subClasses != NULL) {
    /*
     * On a recreate, it might be possible that the newly created class
     * has a different superclass. So we have to flush the precedence
     * list on a recreate as well.
     */
    FlushPrecedences(subClasses);
    NsfClassListFree(subClasses);
  }

  while (cl->super) {
    (void)RemoveSuper(cl, cl->super->cl);
  }

  if (softrecreate == 0) {
    /*
     * flush all caches, unlink superClasses
     */

    while (cl->sub) {
      NsfClass *subClass = cl->sub->cl;
      (void)RemoveSuper(subClass, cl);
      /*
       * If there are no more super classes add the Object
       * class as superClasses
       * -> don't do that for Object itself!
       */
      if (subClass->super == 0 && IsRootClass(cl) == 0) {
        /* fprintf(stderr,"subClass %p %s baseClass %p %s\n",
           cl, ClassName(cl), baseClass, ClassName(baseClass)); */
        AddSuper(subClass, baseClass);
      }
    }
    /*(void)RemoveSuper(cl, cl->super->cl);*/
  }

}

/*
 *----------------------------------------------------------------------
 * CleanupInitClass --
 *
 *    Basic initialization of a class, setting namespace, super- and
 *    sub-classes, and setup optionally instances table.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Makes a class structure useable.
 *
 *----------------------------------------------------------------------
 */
static void CleanupInitClass(Tcl_Interp *interp, NsfClass *cl, Tcl_Namespace *nsPtr,
                 int softrecreate, int recreate) nonnull(1) nonnull(2) nonnull(3);

static void
CleanupInitClass(Tcl_Interp *interp, NsfClass *cl, Tcl_Namespace *nsPtr,
                 int softrecreate, int recreate) {
  NsfClass *defaultSuperclass;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(nsPtr != NULL);
  assert((softrecreate != 0) ? recreate == 1 : 1);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "+++ CleanupInitClass\n");
#endif

  /*
   * Record, that cl is a class and set its namespace
   */
  NsfObjectSetClass((NsfObject *)cl);
  cl->nsPtr = nsPtr;

  if (softrecreate == 0) {
    /*
     * Subclasses are preserved during recreate, superClasses not (since the
     * creation statement defined the superclass, might be different the
     * second time)
     */
    cl->sub = NULL;
  }
  cl->super = NULL;

  /*
   * We can can the default superclass from the metaclass, if this exists.
   */
  if (cl->object.cl != NULL) {
    /* Look for a configured default superclass */
    defaultSuperclass = DefaultSuperClass(interp, cl, cl->object.cl, 0);
  } else {
    defaultSuperclass = NULL;
  }
  if (cl != defaultSuperclass) {
    AddSuper(cl, defaultSuperclass);
  }

  cl->color = WHITE;
  cl->order = NULL;

  if (softrecreate == 0) {
    Tcl_InitHashTable(&cl->instances, TCL_ONE_WORD_KEYS);
    MEM_COUNT_ALLOC("Tcl_InitHashTable", &cl->instances);
  }

  if (recreate == 0) {
    cl->opt = NULL;
  }
}

/*
 *----------------------------------------------------------------------
 * PrimitiveCDestroy --
 *
 *    Delete a class with its namespace and associated data structures. The
 *    physical deallocation is handled by PrimitiveODestroy()
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Free obejct contents.
 *
 *----------------------------------------------------------------------
 */
static void
PrimitiveCDestroy(ClientData clientData) {
  NsfClass *cl;
  NsfObject *object;
  Tcl_Interp *interp;
  Tcl_Namespace *saved;

  nonnull_assert(clientData != NULL);

  cl = (NsfClass *)clientData;
  object = (NsfObject *)clientData;

  PRINTOBJ("PrimitiveCDestroy", object);

  /*
   * check and latch against recurrent calls with obj->teardown
   */
  if (object == NULL || object->teardown == NULL) {
    return;
  }
  interp = object->teardown;

  /*
   * Don't destroy, if the interpreted is destroyed already
   * e.g. TK calls Tcl_DeleteInterp directly, if Window is killed
   */
  if (Tcl_InterpDeleted(interp)) {
    return;
  }

  /*
   * call and latch user destroy with object->id if we haven't
   */
  /*fprintf(stderr, "PrimitiveCDestroy %s flags %.6x\n", ObjectName(object), object->flags);*/

  object->teardown = NULL;
  CleanupDestroyClass(interp, cl, 0, 0);

  /*
   * handoff the primitive teardown
   */
  saved = cl->nsPtr;
  object->teardown = interp;

  /*
   * class object destroy + physical destroy
   */
  PrimitiveODestroy(clientData);

  /*fprintf(stderr, "primitive cdestroy calls delete namespace for obj %p, nsPtr %p flags %.6x\n",
    cl, saved, ((Namespace *)saved)->flags);*/
  Nsf_DeleteNamespace(interp, saved);
  /*fprintf(stderr, "primitive cdestroy %p DONE\n", cl);*/

  return;
}

/*
 *----------------------------------------------------------------------
 * PrimitiveCInit --
 *
 *    Set/reset a class to a fresh, un-destroyed state
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    initializing object structure
 *
 *----------------------------------------------------------------------
 */
static void PrimitiveCInit(NsfClass *cl, Tcl_Interp *interp, const char *name) nonnull(1) nonnull(2) nonnull(3);

static void
PrimitiveCInit(NsfClass *cl, Tcl_Interp *interp, const char *name) {
  Tcl_CallFrame frame, *framePtr = &frame;
  Tcl_Namespace *nsPtr;

  nonnull_assert(cl != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);

  /*
   * Ensure that namespace is newly created during CleanupInitClass. Kill it,
   * if it exists already
   */
  if (Tcl_PushCallFrame(interp, (Tcl_CallFrame *)framePtr,
                        RUNTIME_STATE(interp)->NsfClassesNS, 0) != TCL_OK) {
    return;
  }

  nsPtr = NSGetFreshNamespace(interp, &cl->object, name);
  Tcl_PopCallFrame(interp);

  CleanupInitClass(interp, cl, nsPtr, 0, 0);
  return;
}


/*
 *----------------------------------------------------------------------
 * PrimitiveCCreate --
 *
 *    Allocate memory for a class, initialize the class specific data
 *    structure (eg. class namespace) and call PrimitiveOCreate() for the
 *    object specific initialization.
 *
 * Results:
 *    NsfClass*
 *
 * Side effects:
 *    Allocating memory
 *
 *----------------------------------------------------------------------
 */
static NsfClass *PrimitiveCCreate(Tcl_Interp *interp, Tcl_Obj *nameObj,
                                  Tcl_Namespace *parentNsPtr, NsfClass *metaClass)
  nonnull(1) nonnull(2) returns_nonnull;

static NsfClass *
PrimitiveCCreate(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Namespace *parentNsPtr, NsfClass *metaClass) {
  Tcl_Namespace *nsPtr;
  const char *nameString;
  NsfObject *object;
  NsfClass *cl;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);

  cl = (NsfClass *)ckalloc(sizeof(NsfClass));
  nameString = ObjStr(nameObj);
  object = (NsfObject *)cl;

#if defined(NSFOBJ_TRACE)
  fprintf(stderr, "CKALLOC Class %p %s\n", cl, nameString);
#endif

  memset(cl, 0, sizeof(NsfClass));
  MEM_COUNT_ALLOC("NsfObject/NsfClass", cl);

  /* pass object system from meta class */
  if (metaClass != NULL) {
    cl->osPtr = metaClass->osPtr;
  }

  assert(isAbsolutePath(nameString));
  /*
    fprintf(stderr, "Class alloc %p '%s'\n", cl, nameString);
  */
  nsPtr = NSCheckNamespace(interp, nameString, parentNsPtr);
  if (nsPtr != NULL) {
    NSNamespacePreserve(nsPtr);
  }
#if defined(NRE)
  object->id = Tcl_NRCreateCommand(interp, nameString,
                                   NsfObjDispatch,
                                   NsfObjDispatchNRE,
                                   cl, TclDeletesObject);
#else
  object->id = Tcl_CreateObjCommand(interp, nameString, NsfObjDispatch,
                                    cl, TclDeletesObject);
#endif
  PrimitiveOInit(object, interp, nameString, nsPtr, metaClass);

  if (nsPtr != NULL) {
    NSNamespaceRelease(nsPtr);
  }
  object->cmdName = nameObj;

  /* convert cmdName to Tcl Obj of type cmdName */
  /* Tcl_GetCommandFromObj(interp, obj->cmdName);*/

  INCR_REF_COUNT(object->cmdName);
  PrimitiveCInit(cl, interp, nameString+2);

  ObjTrace("PrimitiveCCreate", object);
  return cl;
}


/*
 *----------------------------------------------------------------------
 * ChangeClass --
 *
 *    Change class of a Next Scripting object. This function takes
 *    care that one tries not to change an object into a class or vice
 *    versa. Changing meta-class to meta-class, or class to class, or
 *    object to object is fine, but upgrading/downgrading is not
 *    allowed
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    changes class of object if possible and updates instance relation.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static int ChangeClass(Tcl_Interp *interp, NsfObject *object, NsfClass *cl)
  nonnull(1) nonnull(2) nonnull(3);

NSF_INLINE static int
ChangeClass(Tcl_Interp *interp, NsfObject *object, NsfClass *cl) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(cl != NULL);

  NsfInstanceMethodEpochIncr("ChangeClass");

  /*fprintf(stderr, "changing %s to class %s ismeta %d\n",
          ObjectName(object), ClassName(cl),
          IsMetaClass(interp, cl, 1));*/

  if (cl != object->cl) {
    if (IsMetaClass(interp, cl, 1)) {
      /* Do not allow upgrading from a class to a meta-class (in
         other words, don't make an object to a class). To allow
         this, it would be necessary to reallocate the base
         structures.
      */
      if (!IsMetaClass(interp, object->cl, 1)) {
        return NsfPrintError(interp, "cannot turn object into a class");
      }
    } else {
      /* The target class is not a meta class.  */

      /*fprintf(stderr, "target class %s not a meta class, am i a class %d\n",
        ClassName(cl), NsfObjectIsClass(object) );*/

      if (NsfObjectIsClass(object)) {
        return NsfPrintError(interp, "cannot turn class into an object ");
      }
    }
    RemoveInstance(object, object->cl);
    AddInstance(object, cl);

    MixinComputeDefined(interp, object);
    FilterComputeDefined(interp, object);
  }
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * DoObjInitialization --
 *
 *    Perform the object initialization: first call "configure" and the
 *    constructor "init", if not called already from configure. The function
 *    will make sure that the called methods do not change the result passed
 *    into this function.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    Indirect effects by calling Tcl code
 *
 *----------------------------------------------------------------------
 */
static int DoObjInitialization(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
DoObjInitialization(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *methodObj, *savedObjResult;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(objv != NULL);
  assert(objc >= 0);

#if 0
  { int i;
    fprintf(stderr, "DoObjInitialization objc %d: ", objc);
    for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
    fprintf(stderr, "\n");
  }
#endif

  /*
   * Save the result we have so far to return it in case of success
   */
  savedObjResult = Tcl_GetObjResult(interp);
  INCR_REF_COUNT(savedObjResult);

  /*
   * clear INIT_CALLED flag
   */
  object->flags &= ~NSF_INIT_CALLED;
  /*
   * Make sure, the object survives initialization; the cmd/initcmd might
   * destroy it.
   */
  NsfObjectRefCountIncr(object);

  /*
   * Call configure method
   */
  if (CallDirectly(interp, object, NSF_o_configure_idx, &methodObj)) {
    NSF_PROFILE_TIME_DATA;
    if (methodObj == NULL) {
      methodObj = NsfGlobalObjs[NSF_CONFIGURE];
    }
    assert(methodObj != NULL);
    /* methodObj is just for error reporting */
    NSF_PROFILE_CALL(interp, object, ObjStr(methodObj));
    result = NsfOConfigureMethod(interp, object, objc, objv, methodObj);
    NSF_PROFILE_EXIT(interp, object, ObjStr(methodObj));
  } else {
    result = CallMethod(object, interp, methodObj, objc+2, objv, NSF_CSC_IMMEDIATE);
  }

  if (likely(result == TCL_OK)) {
    /*
     * Call constructor when needed
     */
    if ((object->flags & (NSF_INIT_CALLED|NSF_DESTROY_CALLED)) == 0u) {
      result = DispatchInitMethod(interp, object, 0, NULL, 0);
    }

    if (likely(result == TCL_OK)) {
      Tcl_SetObjResult(interp, savedObjResult);
    }
  } else {
    /*
     * Configure failed and might have left the object in a bogus state. To
     * avoid strange errors, we delete the half-baked object.
     */
    Tcl_Obj *errObj;

    /*
     *        Preserve the outer error message, calls triggered by
     *  DispatchDestroyMethod() can cause the interp result to be reset
     */

    errObj = Tcl_GetObjResult(interp);
    INCR_REF_COUNT(errObj);

    DispatchDestroyMethod(interp, (NsfObject *)object, 0);

    Tcl_SetObjResult(interp, errObj);
    DECR_REF_COUNT(errObj);
  }

  NsfCleanupObject(object, "obj init");
  DECR_REF_COUNT(savedObjResult);
  return result;
}

/*
 *----------------------------------------------------------------------
 * IsRootMetaClass --
 *
 *    Check, of the class has the Root meta class flag set.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int
IsRootMetaClass(NsfClass *cl) {

  nonnull_assert(cl != NULL);

  return ((cl->object.flags & NSF_IS_ROOT_META_CLASS) != 0u);
}

/*
 *----------------------------------------------------------------------
 * IsBaseClass --
 *
 *    Check, whether the object is a base class.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int
IsBaseClass(NsfObject *object) {

  nonnull_assert(object != NULL);

  return ((object->flags & (NSF_IS_ROOT_CLASS|NSF_IS_ROOT_META_CLASS)) != 0u);
}

/*
 *----------------------------------------------------------------------
 * IsRootClass --
 *
 *    Check, whether the object is a root class.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int
IsRootClass(NsfClass *cls) {

  nonnull_assert(cls != NULL);

  return ((cls->object.flags & (NSF_IS_ROOT_CLASS)) != 0u);
}


/*
 *----------------------------------------------------------------------
 * IsMetaClass --
 *
 *    Check, whether the object is a meta class. Optionally, mixins are
 *    checked as well.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int
IsMetaClass(Tcl_Interp *interp, NsfClass *cl, int withMixins) {
  NsfClasses *pl;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  /* is the class the most general meta-class? */
  if (IsRootMetaClass(cl)) {
    return 1;
  }

  /* is the class a subclass of a meta-class? */
  for (pl = PrecedenceOrder(cl); pl; pl = pl->nextPtr) {
    if (IsRootMetaClass(pl->cl)) {
      return 1;
    }
  }

  if (withMixins == 1) {
    NsfClasses *checkList = NULL, *mixinClasses = NULL, *mc;
    int hasMCM = 0;

    /* has the class metaclass mixed in? */
    NsfClassListAddPerClassMixins(interp, cl, &mixinClasses, &checkList);

    for (mc = mixinClasses; mc; mc = mc->nextPtr) {
      if (IsMetaClass(interp, mc->cl, 0)) {
        hasMCM = 1;
        break;
      }
    }
    if (mixinClasses != NULL) {
      NsfClassListFree(mixinClasses);
    }
    if (checkList != NULL) {
      NsfClassListFree(checkList);
    }
    /*fprintf(stderr, "has MC returns %d, mixinClasses = %p\n",
      hasMCM, mixinClasses);*/

    return hasMCM;
  } else {
    return 0;
  }

}

/*
 *----------------------------------------------------------------------
 * IsSubType --
 *
 *    Check, whether a class is a subclass of another class
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int
IsSubType(NsfClass *subcl, NsfClass *cl) {

  nonnull_assert(subcl != NULL);
  nonnull_assert(cl != NULL);

  if (cl != subcl) {
    return NsfClassListFind(PrecedenceOrder(subcl), cl) != NULL;
  }
  return 1;
}

/*
 *----------------------------------------------------------------------
 * HasMixin --
 *
 *    Check, whether the specified object the the specified class as mixin.
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int HasMixin(Tcl_Interp *interp, NsfObject *object, NsfClass *cl)
  nonnull(1) nonnull(2) nonnull(3);

static int
HasMixin(Tcl_Interp *interp, NsfObject *object, NsfClass *cl) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(cl != NULL);

  if ((object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
    MixinComputeDefined(interp, object);
  }
  if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
    NsfCmdList *ml;

    for (ml = object->mixinOrder; ml; ml = ml->nextPtr) {
      NsfClass *mixin = NsfGetClassFromCmdPtr(ml->cmdPtr);

      if (mixin == cl) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * ImportInstVarIntoCurrentScope --
 *
 *    Import an instance variable into the corrent variable scope
 *    (e.g. function scope).
 *
 * Results:
 *    Boolean
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static int ImportInstVarIntoCurrentScope(Tcl_Interp *interp, const char *cmdName, NsfObject *object,
                                      Tcl_Obj *varName, Tcl_Obj *newName)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
ImportInstVarIntoCurrentScope(Tcl_Interp *interp, const char *cmdName, NsfObject *object,
                           Tcl_Obj *varName, Tcl_Obj *newName) {
  Var *otherPtr = NULL, *arrayPtr;
  unsigned int flogs = TCL_LEAVE_ERR_MSG;
  Tcl_CallFrame *varFramePtr;
  CallFrame frame, *framePtr = &frame;
  const char *varNameString;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(varName != NULL);

  if (unlikely(CheckVarName(interp, ObjStr(varName)) != TCL_OK)) {
    return TCL_ERROR;
  }

  Nsf_PushFrameObj(interp, object, framePtr);
  if (object->nsPtr != NULL) {
    flogs = flogs|TCL_NAMESPACE_ONLY;
  }

  otherPtr = TclObjLookupVar(interp, varName, NULL, flogs, "define",
                             /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
  Nsf_PopFrameObj(interp, framePtr);

  if (unlikely(otherPtr == NULL)) {
    return NsfPrintError(interp, "can't import variable %s into method scope: "
                         "can't find variable on %s",
                         ObjStr(varName), ObjectName(object));
  }

  /*
   * if newName == NULL -> there is no alias, use varName
   * as target link name
   */
  if (newName == NULL) {
    /*
     * Variable link into namespace cannot be an element in an array.
     * see Tcl_VariableObjCmd ...
     */
    if (arrayPtr != NULL) {
      return NsfPrintError(interp, "can't make instance variable %s on %s: "
                           "Variable cannot be an element in an array; use e.g. an alias.",
                           ObjStr(varName), ObjectName(object));
    }

    newName = varName;
  }
  varNameString = ObjStr(newName);
  varFramePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp);

  /*
   * If we are executing inside a Tcl procedure, create a local
   * variable linked to the new namespace variable "varName".
   */
  if (varFramePtr != NULL && (Tcl_CallFrame_isProcCallFrame(varFramePtr) & FRAME_IS_PROC)) {
    Var *varPtr = (Var *)CompiledLocalsLookup((CallFrame *)varFramePtr, varNameString);
    int new = 0;

    if (varPtr == NULL) {
      /*
       * Look in frame's local var hash-table.
       */
      TclVarHashTable *varTablePtr = Tcl_CallFrame_varTablePtr(varFramePtr);

      if (varTablePtr == NULL) {
        /*
         * The variable table does not exist. This seems to be is the
         * first access to a variable on this frame. We create the and
         * initialize the variable hash-table and update the object
         */
        /*fprintf(stderr, "+++ create varTable in ImportInstVarIntoCurrentScope\n");*/
        Tcl_CallFrame_varTablePtr(varFramePtr) = varTablePtr = VarHashTableCreate();
      }
      varPtr = VarHashCreateVar(varTablePtr, newName, &new);
    }

    /*
     * If we define an alias (newName != varName), be sure that
     * the target does not exist already.
     */
    if (new == 0) {
      /*fprintf(stderr, "GetIntoScope create alias\n");*/
      if (unlikely(varPtr == otherPtr)) {
        return NsfPrintError(interp, "can't instvar to variable itself");
      }
      if (TclIsVarLink(varPtr)) {
        /*
         * We try to make the same instvar again ... this is ok
         */
        Var *linkPtr = TclVarValue(Var, varPtr, linkPtr);
        if (linkPtr == otherPtr) {
          return TCL_OK;
        }

        /*fprintf(stderr, "linkvar flags=%x\n", linkPtr->flags);
          Tcl_Panic("new linkvar %s... When does this happen?", ObjStr(newName), NULL);*/

        /*
         * We have already a variable with the same name imported
         * from a different object. Get rid of this old variable.
         */
        VarHashRefCount(linkPtr)--;
        if (TclIsVarUndefined(linkPtr)) {
          TclCleanupVar(linkPtr, (Var *) NULL);
        }

      } else if (unlikely(TclIsVarUndefined(varPtr) == 0)) {
        return NsfPrintError(interp, "varName '%s' exists already", varNameString);
      } else if (unlikely(TclIsVarTraced(varPtr) != 0)) {
        return NsfPrintError(interp, "varName '%s' has traces: can't use for instvar", varNameString);
      }
    }

    TclSetVarLink(varPtr);
    TclClearVarUndefined(varPtr);
    varPtr->value.linkPtr = otherPtr;
    VarHashRefCount(otherPtr)++;

    /* fprintf(stderr, "defining an alias var='%s' in obj %s fwd %d flags %x isLink %d isTraced %d isUndefined %d\n",
            ObjStr(newName), ObjectName(object),
            0,
            varPtr->flags,
            TclIsVarLink(varPtr), TclIsVarTraced(varPtr), TclIsVarUndefined(varPtr));
    */
  } else {
    return NsfPrintError(interp, "%s cannot import variable '%s' into method scope; "
                         "not called from a method frame", cmdName, varNameString);
  }
  return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 * SetInstVar --
 *
 *    Set an instance variable of the specified object to the given value.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Set instance variable.
 *
 *----------------------------------------------------------------------
 */
static int
SetInstVar(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *nameObj, Tcl_Obj *valueObj) {
  CallFrame frame, *framePtr = &frame;
  Tcl_Obj *resultObj;
  unsigned int flags;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(nameObj != NULL);

  flags = (object->nsPtr) ? TCL_LEAVE_ERR_MSG|TCL_NAMESPACE_ONLY : TCL_LEAVE_ERR_MSG;
  Nsf_PushFrameObj(interp, object, framePtr);

  if (likely(valueObj == NULL)) {
    resultObj = Tcl_ObjGetVar2(interp, nameObj, NULL, flags);
  } else {
    /*fprintf(stderr, "setvar in obj %s: name %s = %s\n", ObjectName(object), ObjStr(nameObj), ObjStr(value));*/
    resultObj = Tcl_ObjSetVar2(interp, nameObj, NULL, valueObj, flags);
  }
  Nsf_PopFrameObj(interp, framePtr);

  if (likely(resultObj != NULL)) {
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
  }
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 * SetInstArray --
 *
 *    Set an instance variable array of the specified object to the given
 *    value. This function performs essentially an "array set" or "array get"
 *    operation.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Set instance variable.
 *
 *----------------------------------------------------------------------
 */
static int SetInstArray(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *arrayNameObj, Tcl_Obj *valueObj)
  nonnull(1) nonnull(2) nonnull(3);

static int
SetInstArray(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *arrayNameObj, Tcl_Obj *valueObj) {
  CallFrame frame, *framePtr = &frame;
  int result;
  Tcl_Obj *ov[4];

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(arrayNameObj != NULL);

  Nsf_PushFrameObj(interp, object, framePtr);

  ov[0] = NsfGlobalObjs[NSF_ARRAY];
  ov[2] = arrayNameObj;

  INCR_REF_COUNT(arrayNameObj);
  if (valueObj == NULL) {
    /*
     * Perform an array get
     */
    ov[1] = NsfGlobalObjs[NSF_GET];
    result = Tcl_EvalObjv(interp, 3, ov, 0);
  } else {
    /*
     * Perform an array get
     */
    ov[1] = NsfGlobalObjs[NSF_SET];
    ov[3] = valueObj;
    INCR_REF_COUNT(valueObj);
    result = Tcl_EvalObjv(interp, 4, ov, 0);
    DECR_REF_COUNT(valueObj);
  }
  DECR_REF_COUNT(arrayNameObj);
  Nsf_PopFrameObj(interp, framePtr);

  return result;
}


/*
 *----------------------------------------------------------------------
 * UnsetInstVar --
 *
 *    Unset an instance variable of the specified object.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Variable unset.
 *
 *----------------------------------------------------------------------
 */
static int UnsetInstVar(Tcl_Interp *interp, int withNocomplain, NsfObject *object, const char *name)
  nonnull(1) nonnull(3) nonnull(4);

static int
UnsetInstVar(Tcl_Interp *interp, int withNocomplain, NsfObject *object, const char *name) {
  CallFrame frame, *framePtr = &frame;
  unsigned int flags;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(name != NULL);

  flags = (withNocomplain != 0) ? 0 : TCL_LEAVE_ERR_MSG;
  if (object->nsPtr != NULL) {flags |= TCL_NAMESPACE_ONLY;}

  Nsf_PushFrameObj(interp, object, framePtr);
  result = Tcl_UnsetVar2(interp, name, NULL, flags);
  Nsf_PopFrameObj(interp, framePtr);

  return (withNocomplain != 0) ? TCL_OK : result;
}

/*
 *----------------------------------------------------------------------
 * NsfSetterMethod --
 *
 *    This Tcl_ObjCmdProc is called, when a setter method is invoked. A setter
 *    is a method that accesses/modifies a same-named instance variable. If
 *    the setter is called without arguments, it returns the values, if it is
 *    called with one argument, the argument is used as new value.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Can set an instance variable.
 *
 *----------------------------------------------------------------------
 */
static int NsfSetterMethod(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
NsfSetterMethod(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  SetterCmdClientData *cd = (SetterCmdClientData *)clientData;
  NsfObject *object = cd->object;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  if (objc > 2) {
    return NsfObjWrongArgs(interp, "wrong # args", object->cmdName,
                           NsfMethodNamePath(interp, CallStackGetTclFrame(interp, NULL, 1),
                                             NsfMethodName(objv[0])), "?value?");
  }
  if (object == NULL) {
    return NsfDispatchClientDataError(interp, clientData, "object", ObjStr(objv[0]));
  }

  if (cd->paramsPtr != NULL && objc == 2) {
    Tcl_Obj *outObjPtr;
    int result;
    unsigned flags = 0u;
    ClientData checkedData;

    result = ArgumentCheck(interp, objv[1], cd->paramsPtr,
                           RUNTIME_STATE(interp)->doCheckArguments,
                           &flags, &checkedData, &outObjPtr);

    if (likely(result == TCL_OK)) {
      result = SetInstVar(interp, object, objv[0], outObjPtr);
    }

    if ((flags & NSF_PC_MUST_DECR) != 0u) {
      DECR_REF_COUNT2("valueObj", outObjPtr);
    }
    return result;

  } else {
    return SetInstVar(interp, object, objv[0], objc == 2 ? objv[1] : NULL);
  }
}


/*
 *----------------------------------------------------------------------
 * NsfForwardPrintError --
 *
 *    Helper function to print either an error message directly to call the
 *    forwarder specific callback method specified in
 *    tcd->onerror. Background: ForwardArg() is called at runtime to
 *    substitute the argument list. Catching such errors is not conveniently
 *    doable via catch, since it would be necessary to wrap every possible
 *    usage of a forwarder in a catch. Therefore the callback function can be
 *    used to give a sensible error message appropriate for each context.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Potential side effects through the script.
 *
 *----------------------------------------------------------------------
 */
static int
NsfForwardPrintError(Tcl_Interp *interp, ForwardCmdClientData *tcd,
                     int objc, Tcl_Obj *CONST objv[],
                     const char *fmt, ...)
  nonnull(1) nonnull(2) nonnull(5) NSF_attribute_format((printf,5,6));

static int
NsfForwardPrintError(Tcl_Interp *interp, ForwardCmdClientData *tcd,
                     int objc, Tcl_Obj *CONST objv[],
                     const char *fmt, ...) {
  Tcl_DString ds;
  va_list ap;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(tcd != NULL);
  nonnull_assert(fmt != NULL);

  Tcl_DStringInit(&ds);

  va_start(ap, fmt);
  NsfDStringVPrintf(&ds, fmt, ap);
  va_end(ap);

  if (tcd->onerror != NULL) {
    Tcl_Obj *script = Tcl_DuplicateObj(tcd->onerror);
    Tcl_Obj *cmd;

    if (tcd->object != NULL) {
      cmd = Tcl_DuplicateObj(tcd->object->cmdName);
      if (objc > 0) {
        Tcl_ListObjAppendList(interp, cmd,
                              NsfMethodNamePath(interp,
                                                CallStackGetTclFrame(interp, NULL, 1),
                                                MethodName(objv[0])));
        if (objc > 1) {
          Tcl_ListObjAppendElement(interp, cmd,  Tcl_NewListObj(objc-1, objv+1));
        }
      }
    } else {
      cmd = Tcl_NewListObj(objc, objv);
    }

    Tcl_ListObjAppendElement(interp, script,  cmd);
    Tcl_ListObjAppendElement(interp, script,
                             Tcl_NewStringObj(Tcl_DStringValue(&ds),
                                              Tcl_DStringLength(&ds)));
    INCR_REF_COUNT(script);
    result = Tcl_EvalObjEx(interp, script, TCL_EVAL_DIRECT);
    DECR_REF_COUNT(script);
  } else {
    result = NsfPrintError(interp, "%s", Tcl_DStringValue(&ds));
  }

  Tcl_DStringFree(&ds);
  return result;
}

/*
 *----------------------------------------------------------------------
 * ForwardArg --
 *
 *    This function is a helper function of NsfForwardMethod() and processes a
 *    single entry (ForwardArgObj) of the forward spec. Essentially, it
 *    performs the percent substitution of the forward spec.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Updates the provided output arguments.
 *
 *----------------------------------------------------------------------
 */
static int ForwardArg(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                      Tcl_Obj *ForwardArgObj, ForwardCmdClientData *tcd, Tcl_Obj **out,
                      Tcl_Obj **freeList, int *inputArg, int *mapvalue,
                      int firstPosArg, int *outputincr)
  nonnull(1) nonnull(3) nonnull(4) nonnull(5) nonnull(6) nonnull(7) nonnull(8) nonnull(9) nonnull(11);

static int
ForwardArg(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
           Tcl_Obj *forwardArgObj, ForwardCmdClientData *tcd, Tcl_Obj **out,
           Tcl_Obj **freeList, int *inputArg, int *mapvalue,
           int firstPosArg, int *outputincr) {
  const char *ForwardArgString = ObjStr(forwardArgObj), *p;
  int totalargs;
  char c = *ForwardArgString;

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(forwardArgObj != NULL);
  nonnull_assert(tcd != NULL);
  nonnull_assert(out != NULL);
  nonnull_assert(freeList != NULL);
  nonnull_assert(inputArg != NULL);
  nonnull_assert(mapvalue != NULL);
  nonnull_assert(outputincr != NULL);

  totalargs = objc + tcd->nr_args - 1;
  /*
   * Per default every ForwardArgString from the processed list corresponds to
   * exactly one ForwardArgString in the computed final list.
   */
  *outputincr = 1;
  p = ForwardArgString;

  /* fprintf(stderr, "ForwardArg: processing '%s'\n", ForwardArgString);*/

  if (c == '%' && *(ForwardArgString+1) == '@') {
    char *remainder = NULL;
    long pos;

    ForwardArgString += 2;
    pos = strtol(ForwardArgString, &remainder, 0);
    /*fprintf(stderr, "strtol('%s) returned %ld '%s'\n", ForwardArgString, pos, remainder);*/
    if (ForwardArgString == remainder && *ForwardArgString == 'e'
        && !strncmp(ForwardArgString, "end", 3)) {
      pos = -1;
      remainder += 3;
    } else if (pos < 0) {
      pos --;
    }
    if (ForwardArgString == remainder || labs(pos) > totalargs) {
      return NsfForwardPrintError(interp, tcd, objc, objv,
                                  "forward: invalid index specified in argument %s",
                                  ObjStr(forwardArgObj));
    }
    if (!remainder || *remainder != ' ') {
      return NsfForwardPrintError(interp, tcd, objc, objv,
                                  "forward: invalid syntax in '%s'; use: %%@<pos> <cmd>",
                                  ObjStr(forwardArgObj));
    }

    ForwardArgString = ++remainder;
    /*
     * In case we address from the end, we reduct further to distinguish from
     * -1 (void)
     */
    if (pos < 0) {
      pos--;
    }
    /*fprintf(stderr, "remainder = '%s' pos = %ld\n", remainder, pos);*/
    *mapvalue = pos;
    c = *ForwardArgString;
  }

  if (c == '%') {
    Tcl_Obj *list = NULL, **listElements;
    int nrArgs = objc-1, nrPosArgs = objc - firstPosArg, nrElements = 0;
    char c1, *firstActualArgument = nrArgs > 0 ? ObjStr(objv[1]) : NULL;

    c = *++ForwardArgString;
    c1 = *(ForwardArgString+1);

    if (c == 's' && !strcmp(ForwardArgString, "self")) {
      *out = tcd->object->cmdName;
    } else if ((c == 'p' && !strcmp(ForwardArgString, "proc"))
               || (c == 'm' && !strcmp(ForwardArgString, "method"))
               ) {
      const char *methodName = ObjStr(objv[0]);
      /*
       * If we dispatch a method via ".", we do not want to see the "." in the
       * %proc, e.g. for the interceptor slots (such as mixin, ...)
       */
      if (FOR_COLON_RESOLVER(methodName)) {
        *out = Tcl_NewStringObj(methodName + 1, -1);
      } else {
        *out = objv[0];
      }
    } else if (c == '1' && (c1 == '\0' || c1 == ' ')) {

      if (c1 != '\0') {
        if (unlikely(Tcl_ListObjIndex(interp, forwardArgObj, 1, &list) != TCL_OK)) {
          return NsfForwardPrintError(interp, tcd, objc, objv,
                                      "forward: %%1 must be followed by a valid list, given: '%s'",
                                      ObjStr(forwardArgObj));
        }
        if (unlikely(Tcl_ListObjGetElements(interp, list, &nrElements, &listElements) != TCL_OK)) {
          return NsfForwardPrintError(interp, tcd, objc, objv,
                                      "forward: %%1 contains invalid list '%s'",
                                      ObjStr(list));
        }
      } else if (unlikely(tcd->subcommands != NULL)) { /* deprecated part */
        if (Tcl_ListObjGetElements(interp, tcd->subcommands, &nrElements, &listElements) != TCL_OK) {
          return NsfForwardPrintError(interp, tcd, objc, objv,
                                      "forward: %%1 contains invalid list '%s'",
                                      ObjStr(tcd->subcommands));
        }
      } else {
        assert(nrElements <= nrPosArgs);
      }
      /*fprintf(stderr, "nrElements=%d, nra=%d firstPos %d objc %d\n",
        nrElements, nrArgs, firstPosArg, objc);*/

      if (nrElements > nrPosArgs) {
        /*
         * Insert default subcommand depending on number of arguments.
         */
        /*fprintf(stderr, "inserting listElements[%d] '%s'\n", nrPosArgs,
          ObjStr(listElements[nrPosArgs]));*/
        *out = listElements[nrPosArgs];
      } else if (objc <= 1) {

        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "%%1 requires argument; should be \"%s arg ...\"",
                                    ObjStr(objv[0]));
      } else {
        /*fprintf(stderr, "copying %%1: '%s'\n", ObjStr(objv[firstPosArg]));*/
        *out = objv[firstPosArg];
        *inputArg = firstPosArg+1;
      }
    } else if (c == '-') {
      const char *firstElementString;
      int insertRequired, done = 0;

      /*fprintf(stderr, "process flag '%s'\n", firstActualArgument);*/
      if (Tcl_ListObjGetElements(interp, forwardArgObj, &nrElements, &listElements) != TCL_OK) {
        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "forward: '%s' is not a valid list",
                                    ForwardArgString);
      }
      if (nrElements < 1 || nrElements > 2) {
        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "forward: '%s': must contain 1 or 2 arguments",
                                    ForwardArgString);
      }
      firstElementString = ObjStr(listElements[0]);
      firstElementString++; /* we skip the dash */

      if (firstActualArgument && *firstActualArgument == '-') {
        int i;
        /*fprintf(stderr, "we have a flag in first argument '%s'\n", firstActualArgument);*/

        for (i = 1; i < firstPosArg; i++) {
          if (strcmp(firstElementString, ObjStr(objv[i])) == 0) {
            /*fprintf(stderr, "We have a MATCH for '%s' oldInputArg %d\n", ForwardArgString, *inputArg);*/
            *out = objv[i];
            /* %1 will start at a different place. Proceed if necessary to firstPosArg */
            if (*inputArg < firstPosArg) {
              *inputArg = firstPosArg;
            }
            done = 1;
            break;
          }
        }
      }

      if (done == 0) {
        /*
         * We have a flag in the actual arguments that does not match.  We
         * proceed to the actual arguments without dashes.
         */
        if (*inputArg < firstPosArg) {
          *inputArg = firstPosArg;
        }
        /*
         * If the user requested we output the argument also when not
         * given in the argument list.
         */
        if (nrElements == 2
            && Tcl_GetIntFromObj(interp, listElements[1], &insertRequired) == TCL_OK
            && insertRequired) {
          /*
           * No match, but insert of flag is required.
           */
          /*fprintf(stderr, "no match, but insert of %s required\n", firstElementString);*/
          *out = Tcl_NewStringObj(firstElementString, -1);
          *outputincr = 1;
          goto add_to_freelist;
        } else {
          /*
           * No match, no insert of flag required, we skip the forwarder
           * option and output nothing.
           */
          /*fprintf(stderr, "no match, nrElements %d insert req %d\n", nrElements, insertRequired);*/
          *outputincr = 0;
        }
      }

    } else if (c == 'a' && !strncmp(ForwardArgString, "argcl", 4)) {
      if (Tcl_ListObjIndex(interp, forwardArgObj, 1, &list) != TCL_OK) {
        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "forward: %%argclindex must by a valid list, given: '%s'",
                                    ForwardArgString);
      }
      if (Tcl_ListObjGetElements(interp, list, &nrElements, &listElements) != TCL_OK) {
        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "forward: %%argclindex contains invalid list '%s'",
                                    ObjStr(list));
      }
      if (nrArgs >= nrElements) {
        return NsfForwardPrintError(interp, tcd, objc, objv,
                                    "forward: not enough elements in specified list of ARGC argument %s",
                                    ForwardArgString);
      }
      *out = listElements[nrArgs];
    } else if (c == '%') {
      Tcl_Obj *newarg = Tcl_NewStringObj(ForwardArgString, -1);
      *out = newarg;
      goto add_to_freelist;
    } else {
      /*
       * Evaluate the given command
       */
      int result;
      /*fprintf(stderr, "evaluating '%s'\n", ForwardArgString);*/
      if ((result = Tcl_EvalEx(interp, ForwardArgString, -1, 0)) != TCL_OK) {
        return result;
      }
      *out = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
      /*fprintf(stderr, "result = '%s'\n", ObjStr(*out));*/
      goto add_to_freelist;
    }
  } else {
    if (likely(p == ForwardArgString)) {
      *out = forwardArgObj;
    } else {
      Tcl_Obj *newarg = Tcl_NewStringObj(ForwardArgString, -1);
      *out = newarg;
      goto add_to_freelist;
    }
  }
  return TCL_OK;

 add_to_freelist:
  if (*freeList == NULL) {
    *freeList = Tcl_NewListObj(1, out);
    INCR_REF_COUNT2("freeList", *freeList);
  } else {
    Tcl_ListObjAppendElement(interp, *freeList, *out);
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * CallForwarder --
 *
 *    Invoke the method to which the forwarder points. This function receives
 *    the already transformed argument vector, calls the method and performs
 *    error handling.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe through the invoked command.
 *
 *----------------------------------------------------------------------
 */

static int CallForwarder(ForwardCmdClientData *tcd, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
CallForwarder(ForwardCmdClientData *tcd, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  int result;
  NsfObject *object;
  CallFrame frame, *framePtr = &frame;

  nonnull_assert(tcd != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  object = tcd->object;
  tcd->object = NULL;

  if (unlikely(tcd->verbose)) {
    Tcl_Obj *cmd = Tcl_NewListObj(objc, objv);

    NsfLog(interp, NSF_LOG_DEBUG, "forwarder calls '%s'",  ObjStr(cmd));
    DECR_REF_COUNT(cmd);
  }
  if (tcd->frame == FrameObjectIdx) {
    Nsf_PushFrameObj(interp, object, framePtr);
  }
  if (tcd->objProc != NULL) {
    /*fprintf(stderr, "CallForwarder Tcl_NRCallObjProc %p\n", tcd->clientData);*/
    result = Tcl_NRCallObjProc(interp, tcd->objProc, tcd->clientData, objc, objv);
  } else if (TclObjIsNsfObject(interp, tcd->cmdName, &object)) {
    /*fprintf(stderr, "CallForwarder NsfObjDispatch object %s, objc=%d\n",
      ObjStr(tcd->cmdName), objc);*/
    if (likely(objc > 1)) {
      result = ObjectDispatch(object, interp, objc, objv, NSF_CSC_IMMEDIATE);
    } else {
      result = DispatchDefaultMethod(interp, object, objv[0], NSF_CSC_IMMEDIATE);
    }
  } else {
    /*fprintf(stderr, "CallForwarder: no nsf object %s [0] %s\n", ObjStr(tcd->cmdName), ObjStr(objv[0]));*/
    result = Tcl_EvalObjv(interp, objc, objv, 0);
  }

  if (tcd->frame == FrameObjectIdx) {
    Nsf_PopFrameObj(interp, framePtr);
  }

#if defined(NSF_FORWARD_WITH_ONERROR)
  if (unlikely(result == TCL_ERROR && tcd->onerror)) {
    result = NsfForwardPrintError(interp, tcd, objc, objv, "%s",
                                  ObjStr(Tcl_GetObjResult(interp)));
  }
#endif

  return result;
}

/*
 *----------------------------------------------------------------------
 * NsfForwardMethod --
 *
 *    This Tcl_ObjCmdProc is called, when a forwarder is invoked. It performs
 *    argument substitution through ForwardArg() and calls finally the method,
 *    to which the call was forwarded via CallForwarder().
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe through the invoked command.
 *
 *----------------------------------------------------------------------
 */

static int NsfForwardMethod(ClientData clientData, Tcl_Interp *interp,
                   int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
NsfForwardMethod(ClientData clientData, Tcl_Interp *interp,
                   int objc, Tcl_Obj *CONST objv[]) {
  ForwardCmdClientData *tcd = (ForwardCmdClientData *)clientData;
  int result, inputArg = 1;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  if (unlikely(!tcd || !tcd->object)) {
    return NsfDispatchClientDataError(interp, tcd, "object",
                                      objc > 0 ? ObjStr(objv[0]) : "forwarder");
  }

  /*
   * First, we handle two short cuts for simple cases.
   */

  if (tcd->passthrough != 0) {
    /*
     * This is set for early binding. This means, that the cmd is already
     * resolved, we have to care only for objscope.
     */
    return CallForwarder(tcd, interp, objc, objv);

  } else if (tcd->args == NULL && *(ObjStr(tcd->cmdName)) != '%') {
    /*
     * We have have no args, therefore we have only to replace the method name
     * with the given cmd name.
     */
    ALLOC_ON_STACK(Tcl_Obj*, objc, ov);
    /*fprintf(stderr, "+++ forwardMethod must subst oc=%d <%s>\n",
      objc, ObjStr(tcd->cmdName));*/
    memcpy(ov, objv, sizeof(Tcl_Obj *)*objc);
    ov[0] = tcd->cmdName;
    result = CallForwarder(tcd, interp, objc, ov);
    FREE_ON_STACK(Tcl_Obj *, ov);
    return result;

  } else {
    Tcl_Obj **ov, *freeList = NULL;
    int j, outputincr, outputArg = 0, firstPosArg=1,
      totalargs = objc + tcd->nr_args + 3;

    ALLOC_ON_STACK(Tcl_Obj*, totalargs, OV);
    ALLOC_ON_STACK(int, totalargs, objvmap);

    /*fprintf(stderr, "+++ forwardMethod standard case, allocated %d args, tcd->args %s\n",
      totalargs, ObjStr(tcd->args));*/

    ov = &OV[1];
    if (tcd->needobjmap != 0) {
      memset(objvmap, -1, sizeof(int)*totalargs);
    }

    /*
     * The first argument is always the command, to which we forward.
     */
    if ((result = ForwardArg(interp, objc, objv, tcd->cmdName, tcd,
                             &ov[outputArg], &freeList, &inputArg,
                             &objvmap[outputArg],
                             firstPosArg, &outputincr)) != TCL_OK) {
      goto exitforwardmethod;
    }
    outputArg += outputincr;

    /*
     * If we have nonpos args, determine the first pos arg position for %1
     */
    if (tcd->hasNonposArgs != 0) {
      firstPosArg = objc;
      for (j = outputArg; j < objc; j++) {
        const char *arg = ObjStr(objv[j]);
        if (*arg != '-') {
          firstPosArg = j;
          break;
        }
      }
    }

    if (tcd->args != NULL) {
      Tcl_Obj **listElements;
      int nrElements;

      /*
       * Copy argument list from the definitions.
       */
      Tcl_ListObjGetElements(interp, tcd->args, &nrElements, &listElements);

      for (j = 0; j < nrElements; j++, outputArg += outputincr) {
        if ((result = ForwardArg(interp, objc, objv, listElements[j], tcd,
                                 &ov[outputArg], &freeList, &inputArg,
                                 &objvmap[outputArg],
                                 firstPosArg, &outputincr)) != TCL_OK) {
          goto exitforwardmethod;
        }
      }
    }

    /*fprintf(stderr, "objc=%d, tcd->nr_subcommands=%d size=%d\n",
      objc, tcd->nr_subcommands, objc+ 2            );*/

    if (objc-inputArg > 0) {
      /*fprintf(stderr, "  copying remaining %d args starting at [%d]\n",
        objc-inputArg, outputArg);*/
      memcpy(ov+outputArg, objv+inputArg, sizeof(Tcl_Obj *)*(objc-inputArg));
    } else {
      /*fprintf(stderr, "  nothing to copy, objc=%d, inputArg=%d\n", objc, inputArg);*/
    }
    if (tcd->needobjmap != 0) {
      /*
       * The objmap can shuffle the argument list. We have to set the
       * addressing relative from the end; -2 means last, -3 element before
       * last, etc.
       */
      int max = objc + tcd->nr_args - inputArg;

      for (j = 0; j < totalargs; j++) {
        if (objvmap[j] < -1) {
          /*fprintf(stderr, "must reduct, v=%d\n", objvmap[j]);*/
          objvmap[j] = max + objvmap[j] + 2;
          /*fprintf(stderr, "... new value=%d, max = %d\n", objvmap[j], max);*/
        }
      }
    }
    objc += outputArg - inputArg;

#if 0
    for(j = 0; j < objc; j++) {
      /*fprintf(stderr, "  ov[%d]=%p, objc=%d\n", j, ov[j], objc);*/
      fprintf(stderr, " o[%d]=%p %s (%d),", j, ov[j], ov[j] ? ObjStr(ov[j]) : "NADA", objvmap[j]);
    }
    fprintf(stderr, "\n");
#endif

    if (tcd->needobjmap != 0) {

      for (j = 0; j < totalargs; j++) {
        Tcl_Obj *tmp;
        int pos = objvmap[j], i;
        if (pos == -1 || pos == j) {
          continue;
        }
        tmp = ov[j];
        if (j > pos) {
          for(i = j; i > pos; i--) {
            /*fprintf(stderr, "...moving right %d to %d\n", i-1, i);*/
            ov[i] = ov[i-1];
            objvmap[i] = objvmap[i-1];
          }
        } else {
          for(i = j; i < pos; i++) {
            /*fprintf(stderr, "...moving left %d to %d\n", i+1, i);*/
            ov[i] = ov[i+1];
            objvmap[i] = objvmap[i+1];
          }
        }
        /*fprintf(stderr, "...setting at %d -> %s\n", pos, ObjStr(tmp));*/
        ov[pos] = tmp;
        objvmap[pos] = -1;
      }
    }

    /*
       If a prefix is provided, it will be prepended to the 2nd argument. This
       allows for avoiding name clashes if the 2nd argument denotes a
       subcommand, for example.

       Make sure that the prefix is only prepended, if a second arg is
       actually available! Otherwise, the requested prefix has no effect.
    */
    if (tcd->prefix && objc > 1) {
        Tcl_Obj *methodName = Tcl_DuplicateObj(tcd->prefix);
        Tcl_AppendObjToObj(methodName, ov[1]);
        ov[1] = methodName;
        INCR_REF_COUNT(ov[1]);
    }

#if 0
    for(j = 0; j < objc; j++) {
      /*fprintf(stderr, "  ov[%d]=%p, objc=%d\n", j, ov[j], objc);*/
      fprintf(stderr, "  ov[%d]=%p '%s' map=%d\n", j, ov[j], ov[j] ? ObjStr(ov[j]) : "NADA", objvmap[j]);
    }
#endif

    OV[0] = tcd->cmdName;

    result = CallForwarder(tcd, interp, objc, ov);

    if (tcd->prefix && objc > 1) {
      DECR_REF_COUNT(ov[1]);
    }
  exitforwardmethod:
    if (freeList != NULL)    {DECR_REF_COUNT2("freeList", freeList);}

    FREE_ON_STACK(int, objvmap);
    FREE_ON_STACK(Tcl_Obj*, OV);
  }
  return result;
}


/*
 *----------------------------------------------------------------------
 * NsfProcAliasMethod --
 *
 *    Since alias-resolving happens in dispatch, this Tcl_ObjCmdProc should
 *    never be called during normal operations. The only way to invoke this
 *    could happen via directly calling the handle.
 *
 * Results:
 *    TCL_ERROR
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int NsfProcAliasMethod(ClientData clientData,
                              Tcl_Interp *interp, int objc,
                              Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
NsfProcAliasMethod(ClientData clientData,
                     Tcl_Interp *interp, int objc,
                     Tcl_Obj *CONST objv[]) {
  AliasCmdClientData *tcd = (AliasCmdClientData *)clientData;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  return NsfDispatchClientDataError(interp, NULL, "object",
                                    Tcl_GetCommandName(interp, tcd->aliasCmd));
}


/*
 *----------------------------------------------------------------------
 * NsfObjscopedMethod --
 *
 *    This Tcl_ObjCmdProc is called, when an objscoped alias is invoked.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe through the invoked command.
 *
 *----------------------------------------------------------------------
 */

static int NsfObjscopedMethod(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
  nonnull(1) nonnull(2) nonnull(4);

static int
NsfObjscopedMethod(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  AliasCmdClientData *tcd = (AliasCmdClientData *)clientData;
  NsfObject *object = tcd->object;
  CallFrame frame, *framePtr = &frame;
  int result;

  nonnull_assert(clientData != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  /*fprintf(stderr, "objscopedMethod obj=%p %s, ptr=%p\n", object, ObjectName(object), tcd->objProc);*/
  tcd->object = NULL;

  Nsf_PushFrameObj(interp, object, framePtr);
  result = Tcl_NRCallObjProc(interp, tcd->objProc, tcd->clientData, objc, objv);
  Nsf_PopFrameObj(interp, framePtr);

  return result;
}

/*
 *----------------------------------------------------------------------
 * IsDashArg --
 *
 *    Check, whether the provided argument (pointed to be the index isFirstArg)
 *    starts with a "-", or is a list starting with a "-". The method returns
 *    via **methodName the name of the dashed argument (without the dash).
 *
 * Results:
 *    Enum value dashArgType.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

typedef enum {NO_DASH, SKALAR_DASH, LIST_DASH} dashArgType;

static dashArgType IsDashArg(Tcl_Interp *interp, Tcl_Obj *obj, int isFirstArg, const char **methodName,
          int *objc, Tcl_Obj **objv[]) nonnull(1) nonnull(2) nonnull(4) nonnull(5) nonnull(6);

static dashArgType
IsDashArg(Tcl_Interp *interp, Tcl_Obj *obj, int isFirstArg, const char **methodName,
          int *objcPtr, Tcl_Obj **objvPtr[]) {
  const char *flag;

  nonnull_assert(interp != NULL);
  nonnull_assert(obj != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(objcPtr != NULL);
  nonnull_assert(objvPtr != NULL);

  if (obj->typePtr == Nsf_OT_listType) {
    if (Tcl_ListObjGetElements(interp, obj, objcPtr, objvPtr) == TCL_OK && *objcPtr > 1) {
      flag = ObjStr(*objvPtr[0]);
      /*fprintf(stderr, "we have a list starting with '%s'\n", flag);*/
      if (*flag == '-') {
        *methodName = flag+1;
        return LIST_DASH;
      }
    }
  }
  flag = ObjStr(obj);
  /*fprintf(stderr, "we have a scalar '%s' isFirstArg %d\n", flag, isFirstArg);*/

  if ((*flag == '-') && isalpha(*((flag)+1))) {
    if (isFirstArg == 1) {
      /* if the argument contains a space, try to split */
      const char *p= flag+1;
      while (*p && *p != ' ') p++;
      if (*p == ' ') {
        if (Tcl_ListObjGetElements(interp, obj, objcPtr, objvPtr) == TCL_OK) {
          *methodName = ObjStr(*objvPtr[0]);
          if (**methodName == '-') {(*methodName)++ ;}
          return LIST_DASH;
        }
      }
    }
    *methodName = flag+1;
    *objcPtr = 1;
    return SKALAR_DASH;
  }
  return NO_DASH;
}

/*
 *----------------------------------------------------------------------
 * CallConfigureMethod --
 *
 *    Call a method identified by a string selector; or provide an error
 *    message. This dispatcher function records as well constructor (init)
 *    calls via this interface. The dispatcher is used in XOTcl's
 *    configure(), interpreting arguments with a leading dash as method dispatches.
 *    This behavior is now implemented in NsfOResidualargsMethod().
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Maybe side effects from the called methods.
 *
 *----------------------------------------------------------------------
 */
static int CallConfigureMethod(Tcl_Interp *interp, NsfObject *object, const char *initString,
                               const char *methodName,
                               int argc, Tcl_Obj *CONST argv[])
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

static int
CallConfigureMethod(Tcl_Interp *interp, NsfObject *object, const char *initString,
                    const char *methodName,
                    int argc, Tcl_Obj *CONST argv[]) {
  int result;
  Tcl_Obj *methodObj = Tcl_NewStringObj(methodName, -1);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(initString != NULL);
  nonnull_assert(methodName != NULL);

  /*
   * When configure gets "-init" passed, we call "init" and notice the fact it
   * in the object's flags.
   */

  if (*initString == *methodName && strcmp(methodName, initString) == 0) {
    object->flags |= NSF_INIT_CALLED;
  }

  Tcl_ResetResult(interp);
  INCR_REF_COUNT(methodObj);
  result = CallMethod(object, interp, methodObj, argc, argv,
                      NSF_CM_NO_UNKNOWN|NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS);
  DECR_REF_COUNT(methodObj);

  /*fprintf(stderr, "method  '%s' called args: %d o=%p, result=%d %d\n",
    methodName, argc+1, object, result, TCL_ERROR);*/

  if (unlikely(result != TCL_OK)) {
    Tcl_Obj *res = Tcl_DuplicateObj(Tcl_GetObjResult(interp)); /* save the result */

    INCR_REF_COUNT(res);
    NsfPrintError(interp, "%s during '%s.%s'", ObjStr(res), ObjectName(object), methodName);
    DECR_REF_COUNT(res);
  }

  return result;
}


/*
 * class method implementations
 */

/*
 *----------------------------------------------------------------------
 * IsRootNamespace --
 *
 *    Check, if the provided namespace is the namespace of the base
 *    class of an object system.
 *
 * Results:
 *    Boolean value.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int IsRootNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1) nonnull(2);

static int
IsRootNamespace(Tcl_Interp *interp, Tcl_Namespace *nsPtr) {
  NsfObjectSystem *osPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);

  for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = osPtr->nextPtr) {
    Tcl_Command cmd = osPtr->rootClass->object.id;
    if ((Tcl_Namespace *)((Command *)cmd)->nsPtr == nsPtr) {
      return 1;
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * CallingNameSpace --
 *
 *    Find the last invocation outside the Next Scripting system
 *    namespaces. This function return the namespace of the caller but
 *    skips system-specific namespaces (e.g. the namespaces of the
 *    pre-defined slot handlers for mixin and class
 *    registration. etc.) If we would use such namespaces, we would
 *    resolve non-fully-qualified names against the root namespace).
 *
 * Results:
 *    Tcl_Namespace or NULL
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Namespace *
CallingNameSpace(Tcl_Interp *interp) {
  Tcl_CallFrame *framePtr;
  Tcl_Namespace *nsPtr;

  nonnull_assert(interp != NULL);

  /*NsfShowStack(interp);*/
  framePtr = CallStackGetActiveProcFrame((Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp));
  /* framePtr = BeginOfCallChain(interp, GetSelfObj(interp));*/

  for (; likely(framePtr != NULL); framePtr = Tcl_CallFrame_callerVarPtr(framePtr)) {
    nsPtr = Tcl_CallFrame_nsPtr(framePtr);

    if (IsRootNamespace(interp, nsPtr)) {
      /*fprintf(stderr, "... %p skip %s\n", framePtr, nsPtr->fullName);*/
      continue;
    }
    /*fprintf(stderr, "... %p take %s\n", framePtr, nsPtr->fullName); */
    break;
  }

  if (framePtr == NULL) {
    nsPtr = Tcl_GetGlobalNamespace(interp);
  }

  /*fprintf(stderr, " **** CallingNameSpace: returns %p %s framePtr %p\n",
    nsPtr, (nsPtr != NULL) ? nsPtr->fullName:"(null)", framePtr);*/
  return nsPtr;
}

/***********************************
 * argument handling
 ***********************************/

static void ArgumentResetRefCounts(struct Nsf_Param const *pPtr, Tcl_Obj *valueObj) nonnull(1) nonnull(2);

static void
ArgumentResetRefCounts(struct Nsf_Param const *pPtr, Tcl_Obj *valueObj) {

  nonnull_assert(pPtr != NULL);
  nonnull_assert(valueObj != NULL);

  if ((pPtr->flags & NSF_ARG_IS_CONVERTER) != 0u) {
    DECR_REF_COUNT2("valueObj", valueObj);
  }
}

/*
 *----------------------------------------------------------------------
 * ArgumentCheckHelper --
 *
 *    Helper function for ArgumentCheck() called when argument checking leads
 *    to a different output element (non-pure checking).
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int ArgumentCheckHelper(Tcl_Interp *interp, Tcl_Obj *objPtr, struct Nsf_Param const *pPtr, unsigned int *flags,
                               ClientData *clientData, Tcl_Obj **outObjPtr)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5) nonnull(6);

static int
ArgumentCheckHelper(Tcl_Interp *interp, Tcl_Obj *objPtr, struct Nsf_Param const *pPtr, unsigned int *flags,
                    ClientData *clientData, Tcl_Obj **outObjPtr) {
  int objc, i, result;
  Tcl_Obj **ov;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(flags != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);
  assert((pPtr->flags & NSF_ARG_MULTIVALUED) != 0u);
  assert(*flags & NSF_PC_MUST_DECR);

  result = Tcl_ListObjGetElements(interp, objPtr, &objc, &ov);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  *outObjPtr = Tcl_NewListObj(0, NULL);
  INCR_REF_COUNT2("valueObj", *outObjPtr);

  for (i = 0; i < objc; i++) {
    Tcl_Obj *elementObjPtr = ov[i];
    const char *valueString = ObjStr(elementObjPtr);

    if ((pPtr->flags & NSF_ARG_ALLOW_EMPTY) != 0u && *valueString == '\0') {
      result = Nsf_ConvertToString(interp, elementObjPtr, pPtr, clientData, &elementObjPtr);
    } else {
      result = (*pPtr->converter)(interp, elementObjPtr, pPtr, clientData, &elementObjPtr);
    }

    /*fprintf(stderr, "ArgumentCheckHelper convert %s result %d (%s)\n",
      valueString, result, ObjStr(elementObjPtr));*/

    if (result == TCL_OK || result == TCL_CONTINUE) {
      Tcl_ListObjAppendElement(interp, *outObjPtr, elementObjPtr);
      /*
       * If the refCount of the valueObj was already incremented, we have to
       * decrement it here, since we want the valuObj reclaimed when the list
       * containing the valueObj is freed.
       */
      ArgumentResetRefCounts(pPtr, elementObjPtr);
    } else {
      Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

      INCR_REF_COUNT(resultObj);
      NsfPrintError(interp, "invalid value in \"%s\": %s", ObjStr(objPtr), ObjStr(resultObj));
      *flags &= ~NSF_PC_MUST_DECR;
      *outObjPtr = objPtr;
      DECR_REF_COUNT2("valueObj", *outObjPtr);
      DECR_REF_COUNT(resultObj);

      break;
    }
  }
  return result;
}


/*
 *----------------------------------------------------------------------
 * ArgumentCheck --
 *
 *    Check a single argument (2nd argument) against the parameter structure
 *    when argument checking is turned on (default).
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int
ArgumentCheck(Tcl_Interp *interp, Tcl_Obj *objPtr, struct Nsf_Param const *pPtr,
              int doCheckArguments,
              unsigned int *flags, ClientData *clientData, Tcl_Obj **outObjPtr) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);
  nonnull_assert(pPtr != NULL);
  nonnull_assert(flags != NULL);
  nonnull_assert(clientData != NULL);
  nonnull_assert(outObjPtr != NULL);

  /*
   * Default assumption: outObjPtr is not modified.
   */
  *outObjPtr = objPtr;

  /*
   * Omit argument checking, provided that ...
   * ... argument checking is turned off *and* no converter is specified, or
   * ... the ruling parameter option is 'initcmd'
   */
  if ((unlikely((doCheckArguments & NSF_ARGPARSE_CHECK) == 0u)
       && (pPtr->flags & (NSF_ARG_IS_CONVERTER)) == 0u
       ) || ((pPtr->flags & (NSF_ARG_CMD)) != 0u)) {
    /* fprintf(stderr, "*** omit  argument check for arg %s flags %.6x\n", pPtr->name, pPtr->flags); */
    *clientData = ObjStr(objPtr);
    return TCL_OK;
  }

  /*
   * If the argument is multivalued, perform the check for every element of
   * the list (pure checker), or we have to build a new list of values (in
   * case, the converter alters the values).
   */
  if (unlikely((pPtr->flags & NSF_ARG_MULTIVALUED) != 0u)) {
    int objc, i;
    Tcl_Obj **ov;

    result = Tcl_ListObjGetElements(interp, objPtr, &objc, &ov);
    if (unlikely(result != TCL_OK)) {
      return result;
    }

    if (objc == 0 && ((pPtr->flags & NSF_ARG_ALLOW_EMPTY) == 0u)) {
      return NsfPrintError(interp,
                           "invalid value for parameter '%s': list is not allowed to be empty",
                           pPtr->name);
    }

    /*
     * In cases where necessary (the output element changed), switch to the
     * helper function
     */
    for (i = 0; i < objc; i++) {
      Tcl_Obj *elementObjPtr = ov[i];

      result = (*pPtr->converter)(interp, elementObjPtr, pPtr, clientData, &elementObjPtr);
      if (likely(result == TCL_OK || result == TCL_CONTINUE)) {
        if (ov[i] != elementObjPtr) {
          /*fprintf(stderr, "ArgumentCheck: switch to output list construction for value %s\n",
            ObjStr(elementObjPtr));*/
          /*
           * The elementObjPtr differs from the input Tcl_Obj, we switch to
           * the version of this handler building an output list. But first,
           * we have to reset the ref-counts from the first conversion.
           */
          ArgumentResetRefCounts(pPtr, elementObjPtr);
          *flags |= NSF_PC_MUST_DECR;
          result = ArgumentCheckHelper(interp, objPtr, pPtr, flags, clientData, outObjPtr);
          break;
        }
      } else {
        Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
        INCR_REF_COUNT(resultObj);
        NsfPrintError(interp, "invalid value in \"%s\": %s", ObjStr(objPtr), ObjStr(resultObj));
        DECR_REF_COUNT(resultObj);
        break;
      }
    }
  } else {
    assert(objPtr == *outObjPtr);
    if ((pPtr->flags & NSF_ARG_ALLOW_EMPTY) != 0u && *(ObjStr(objPtr)) == '\0') {
      result = Nsf_ConvertToString(interp, objPtr, pPtr, clientData, outObjPtr);
    } else {
      result = (*pPtr->converter)(interp, objPtr, pPtr, clientData, outObjPtr);
    }

    /*fprintf(stderr, "ArgumentCheck param %s type %s is converter %d flags %.6x "
            "outObj changed %d (%p %p) isok %d\n",
            pPtr->name, pPtr->type, pPtr->flags & NSF_ARG_IS_CONVERTER, pPtr->flags,
            objPtr != *outObjPtr, objPtr, *outObjPtr, result == TCL_OK);*/

    if (unlikely((pPtr->flags & NSF_ARG_IS_CONVERTER) != 0u) && objPtr != *outObjPtr) {
      *flags |= NSF_PC_MUST_DECR;
    } else {
      /*
       * If the output obj differs from the input obj, ensure we have
       * MUST_DECR set.
       */
      assert( (*flags & NSF_PC_MUST_DECR) != 0u || objPtr == *outObjPtr );
    }
  }

  if (unlikely(result == TCL_CONTINUE)) {
    *flags |= NSF_ARG_WARN;
    result = TCL_OK;
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * ArgumentDefaults --
 *
 *    Process the argument vector and set defaults in parse context if
 *    provided and necessary.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int ArgumentDefaults(ParseContext *pcPtr, Tcl_Interp *interp,
                            Nsf_Param const *ifd, int nrParams, int processFlags)
  nonnull(1) nonnull(2) nonnull(3);

static int
ArgumentDefaults(ParseContext *pcPtr, Tcl_Interp *interp,
                 Nsf_Param const *ifd, int nrParams, int processFlags) {
  Nsf_Param const *pPtr;
  int i;

  nonnull_assert(pcPtr != NULL);
  nonnull_assert(interp != NULL);
  nonnull_assert(ifd != NULL);

  for (pPtr = ifd, i = 0; i < nrParams; pPtr++, i++) {
    /*fprintf(stderr, "ArgumentDefaults got for arg %s (req %d, nrArgs %d) %p => %p %p, default '%s' \n",
            pPtr->name, pPtr->flags & NSF_ARG_REQUIRED, pPtr->nrArgs, pPtr,
            pcPtr->clientData[i], pcPtr->objv[i], (pPtr->defaultValue != NULL) ? ObjStr(pPtr->defaultValue) : "NONE");*/

    if (pcPtr->objv[i]) {
      /*
       * We got an actual value, which was already checked by ArgumentParse().
       * In case the value is a switch and NSF_PC_INVERT_DEFAULT is set, we
       * take the default and invert the value in place.
       */
      if (unlikely(pcPtr->flags[i] & NSF_PC_INVERT_DEFAULT)) {
        int bool;

        Tcl_GetBooleanFromObj(interp, pPtr->defaultValue, &bool);
        pcPtr->objv[i] = Tcl_NewBooleanObj(bool == 0);
        /*
         * Perform bookkeeping to avoid that someone releases the new obj
         * before we are done. The according DECR is performed by
         * ParseContextRelease()
         */
        INCR_REF_COUNT2("valueObj", pcPtr->objv[i]);
        pcPtr->flags[i] |= NSF_PC_MUST_DECR;
        pcPtr->status |= NSF_PC_STATUS_MUST_DECR;
      }
    } else {
      /*
       * No valued was passed, check if a default is available.
       */

      if (pPtr->defaultValue != NULL) {
        int mustDecrNewValue;
        Tcl_Obj *newValue = pPtr->defaultValue;
        ClientData checkedData;

        /*
         * We have a default value for the argument.  Mark that this argument
         * gets the default value.
         */
        pcPtr->flags[i] |= NSF_PC_IS_DEFAULT;

        /*
         * Does the user want to substitute in the default value?
         */
        if (unlikely((pPtr->flags & NSF_ARG_SUBST_DEFAULT) != 0u)) {
          Tcl_Obj *obj = Tcl_SubstObj(interp, newValue, TCL_SUBST_ALL);

          if (likely(obj != NULL)) {
             newValue = obj;
          } else {
            pcPtr->flags[i] = 0;
            return TCL_ERROR;
          }

          /* The matching DECR is performed by ParseContextRelease() */
          INCR_REF_COUNT2("valueObj", newValue);
          /*fprintf(stderr, "SUBST_DEFAULT increments %p refCount %d\n",
            newValue, newValue->refCount);*/
          mustDecrNewValue = 1;
          pcPtr->flags[i] |= NSF_PC_MUST_DECR;
          pcPtr->status |= NSF_PC_STATUS_MUST_DECR;
        } else {
          mustDecrNewValue = 0;
        }

        pcPtr->objv[i] = newValue;
        /*fprintf(stderr, "==> setting default value '%s' for var '%s' flag %d type %s conv %p\n",
                ObjStr(newValue), pPtr->name, pPtr->flags & NSF_ARG_INITCMD,
                pPtr->type, pPtr->converter);*/
        /*
         * Check the default value if necessary
         */
        if (pPtr->type != NULL || unlikely((pPtr->flags & NSF_ARG_MULTIVALUED) != 0u)) {
          unsigned int mustDecrList = 0;

          if (unlikely((pPtr->flags & NSF_ARG_INITCMD) == 0u &&
                       ArgumentCheck(interp, newValue, pPtr,
                                     RUNTIME_STATE(interp)->doCheckArguments,
                                     &mustDecrList, &checkedData, &pcPtr->objv[i]) != TCL_OK)) {
            if (mustDecrNewValue == 1) {
              DECR_REF_COUNT2("valueObj", newValue);
              pcPtr->flags[i] &= ~NSF_PC_MUST_DECR;
            }
            return TCL_ERROR;
          }

          if (unlikely(pcPtr->objv[i] != newValue)) {
            /*
             * The output Tcl_Obj differs from the input, so the
             * Tcl_Obj was converted; in case we have set previously
             * must_decr on newValue, we decr the refCount on newValue
             * here and clear the flag.
             */
            if (mustDecrNewValue == 1) {
              DECR_REF_COUNT2("valueObj", newValue);
              pcPtr->flags[i] &= ~NSF_PC_MUST_DECR;
            }
            /*
             * The new output value itself might require a decr, so
             * set the flag here if required; this is just necessary
             * for multivalued converted output.
             */
            if (mustDecrList == 1) {
              pcPtr->flags[i] |= NSF_PC_MUST_DECR;
              pcPtr->status |= NSF_PC_STATUS_MUST_DECR;
            }
          }
        } else {
          /*fprintf(stderr, "Param %s default %s type %s\n",
            pPtr->name, ObjStr(pPtr->defaultValue), pPtr->type);*/
          assert((pPtr->type != NULL) ? pPtr->defaultValue == NULL : 1);
        }
      } else if (unlikely((pPtr->flags & NSF_ARG_REQUIRED) != 0u)
                 && ((processFlags & NSF_ARGPARSE_FORCE_REQUIRED) != 0u)) {
        Tcl_Obj *paramDefsObj = NsfParamDefsSyntax(interp, ifd, pcPtr->object, NULL);
        Tcl_Obj *methodPathObj = NsfMethodNamePath(interp,
                                                   CallStackGetTclFrame(interp, NULL, 1),
                                                   MethodName(pcPtr->full_objv[0]));

        INCR_REF_COUNT2("methodPathObj", methodPathObj);

        NsfPrintError(interp, "required argument '%s' is missing, should be:\n\t%s%s%s %s", (pPtr->nameObj != NULL) ? ObjStr(pPtr->nameObj) : pPtr->name, (pcPtr->object != NULL) ? ObjectName(pcPtr->object) : "", (pcPtr->object != NULL) ? " " : "",
                      ObjStr(methodPathObj),
                      ObjStr(paramDefsObj));

        DECR_REF_COUNT2("paramDefsObj", paramDefsObj);
        DECR_REF_COUNT2("methodPathObj", methodPathObj);

        return TCL_ERROR;

      } else {
        /*
         * Use as dummy default value an arbitrary symbol, which must
         * not be returned to the Tcl level level; this value is unset
         * later typically by NsfUnsetUnknownArgsCmd().
         */
        pcPtr->objv[i] = NsfGlobalObjs[NSF___UNKNOWN__];
      }
    }
  }
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 * ArgumentParse --
 *
 *    Parse the argument vector based on the parameter definitions.
 *    The parsed argument vector is returned in a normalized order
 *    in the parse context.
 *
 * Results:
 *    Tcl return code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
int
Nsf_ArgumentParse(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
                  Nsf_Object *object, Tcl_Obj *procNameObj,
                  Nsf_Param const *paramPtr, int nrParams, int serial,
                  unsigned int processFlags, Nsf_ParseContext *pcPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(procNameObj != NULL);
  nonnull_assert(pcPtr != NULL);

  return ArgumentParse(interp, objc, objv, (NsfObject *)object, procNameObj,
                       paramPtr, nrParams, serial, processFlags,
                       (ParseContext *)pcPtr);
}

/*
 *----------------------------------------------------------------------
 * NextParam --
 *
 *    Advance in the parameter definitions and return the next parameter.
 *
 * Results:
 *    Next parameter.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Nsf_Param const * NextParam(Nsf_Param const *paramPtr, Nsf_Param const *lastParamPtr)
  nonnull(1) nonnull(2) returns_nonnull;

static Nsf_Param const *
NextParam(Nsf_Param const *paramPtr, Nsf_Param const *lastParamPtr) {

  nonnull_assert(paramPtr != NULL);
  nonnull_assert(lastParamPtr != NULL);

  for (; (++paramPtr <= lastParamPtr) && (*paramPtr->name == '-'); );
  return paramPtr;
}

/*
 *----------------------------------------------------------------------
 * ArgumentParse --
 *
 *    Parse the provided ist of argument against the given definition. The
 *    result is returned in the parameter context structure.
 *
 * Results:
 *    Standard Tcl result.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

#define SkipNonposParamDefs(cPtr) \
  for (; (++(cPtr) <= lastParamPtr) && (*(cPtr)->name == '-'); )

static int
ArgumentParse(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
              NsfObject *object, Tcl_Obj *procNameObj,
              Nsf_Param const *paramPtr, int nrParams, int serial,
              unsigned int processFlags, ParseContext *pcPtr) {
  int o, dashdash = 0, j, fromArg;
  Nsf_Param const *currentParamPtr = paramPtr;
  Nsf_Param const *lastParamPtr = paramPtr + nrParams - 1;

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);
  nonnull_assert(procNameObj != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(pcPtr != NULL);

  if ((processFlags & NSF_ARGPARSE_START_ZERO) != 0u) {
    fromArg = 0;
  } else {
    fromArg = 1;
  }

  ParseContextInit(pcPtr, nrParams, object, procNameObj);

#if defined(PARSE_TRACE)
  { Nsf_Param const *pPtr;
    fprintf(stderr, "PARAMETER ");
    for (o = 0, pPtr = paramPtr; pPtr->name; o++, pPtr++) {
      fprintf(stderr, "[%d]%s (nrargs %d %s) ", o,
              pPtr->name, pPtr->nrArgs,
              (pPtr->flags & NSF_ARG_REQUIRED) != 0u ? "req" : "opt");
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "BEGIN (%d) [0]%s ", objc, ObjStr(procNameObj));
    for (o = fromArg; o < objc; o++) {fprintf(stderr, "[%d]%s ", o, ObjStr(objv[o]));}
    fprintf(stderr, "\n");
  }
#endif

  for (o = fromArg; o < objc; o++) {
    Nsf_Param const *pPtr = currentParamPtr;
    Tcl_Obj *argumentObj = objv[o];
    Tcl_Obj *valueObj = NULL;
    const char *valueInArgument = NULL;

#if defined(PARSE_TRACE_FULL)
    fprintf(stderr, "arg [%d]: %s (param %ld, last %d)\n",
            o, ObjStr(argumentObj), currentParamPtr - paramPtr, currentParamPtr == lastParamPtr);
#endif

    if (unlikely(currentParamPtr > lastParamPtr)) {
      Tcl_Obj *methodPathObj;

      methodPathObj = NsfMethodNamePath(interp,
                                        CallStackGetTclFrame(interp, NULL, 0),
                                        NsfMethodName(procNameObj));
      return NsfUnexpectedArgumentError(interp, ObjStr(argumentObj), (Nsf_Object*)object,
                                        paramPtr, methodPathObj);
    }

    if (*currentParamPtr->name == '-') {
      /*
       * We expect a nonpos arg. Check, if we a Tcl_Obj already converted to
       * NsfFlagObjType.
       */
      NsfFlag *flagPtr = argumentObj->internalRep.twoPtrValue.ptr1;

#if defined(PARSE_TRACE_FULL)
      fprintf(stderr, "... arg %p %s expect nonpos arg in block %s isFlag %d sig %d serial %d (%d => %d)\n",
              argumentObj, ObjStr(argumentObj), currentParamPtr->name,
              argumentObj->typePtr == &NsfFlagObjType,
              argumentObj->typePtr == &NsfFlagObjType ? flagPtr->signature == paramPtr : 0,
              argumentObj->typePtr == &NsfFlagObjType ? flagPtr->serial == serial : 0,
              argumentObj->typePtr == &NsfFlagObjType ? flagPtr->serial : 0,
              serial );
#endif

      if (argumentObj->typePtr == &NsfFlagObjType
          && flagPtr->signature == paramPtr
          && flagPtr->serial == serial
          ) {
        /*
         * The argument was processed before and the Tcl_Obj is still valid.
         */
        if ((flagPtr->flags & NSF_FLAG_DASHDAH) != 0u) {
          /*
           * We got a dashDash, skip nonpos param definitions and continue with next
           * element from objv.
           */
          SkipNonposParamDefs(currentParamPtr);
          assert(dashdash == 0);
          continue;
        } else if ((flagPtr->flags & NSF_FLAG_CONTAINS_VALUE) != 0u) {
          /*
           * We got a flag with an embedded value (e.g -flag=1).
           */
          valueInArgument = "flag";
        }
        pPtr = flagPtr->paramPtr;
        valueObj = flagPtr->payload;

      } else {
        const char *argumentString = ObjStr(argumentObj);
        /*
         * We are in a state, where we expect a non-positional argument, and
         * the lookup from the Tcl_Obj has failed.  If this non-pos args are
         * optional, the current argument might contain also a value for a
         * positional argument maybe the argument is for a posarg
         * later). First check, if the argument looks like a flag.
         */
        if (argumentString[0] != '-') {
          /*
           * The actual argument is not a flag, so proceed in the parameter
           * vector to the next block (positional parameter)
           */
          SkipNonposParamDefs(currentParamPtr);
          pPtr = currentParamPtr;
          /*
           * currentParamPtr is either NULL or points to a positional parameter
           */
          assert(currentParamPtr == NULL || currentParamPtr->name == NULL || *currentParamPtr->name != '-');

        } else {
          /*
           * The actual argument starts with a dash, so search for the flag in
           * the current block of nonpos parameter definitions
           */
          char ch1 = *(argumentString+1);

          /* Is there a "--" ? */
          if (ch1 == '-' && *(argumentString+2) == '\0' && dashdash == 0) {
            dashdash = 1;
            NsfFlagObjSet(interp, argumentObj, paramPtr, serial,
                          NULL, NULL, NSF_FLAG_DASHDAH);
            SkipNonposParamDefs(currentParamPtr);
            continue;
          }

          valueInArgument = strchr(argumentString, '=');
          if (valueInArgument != NULL) {
            int found = 0, equalOffset = valueInArgument - argumentString;

            /*
             * Handle parameter like -flag=1
             */
            for (; (pPtr <= lastParamPtr) && (*pPtr->name == '-'); pPtr++) {
              if (pPtr->nrArgs > 0) {
                /* parameter expects no arg, can't be this */
                continue;
              }
              if ((pPtr->flags & NSF_ARG_NOCONFIG) == 0u
                  && ch1 == pPtr->name[1]
                  && strncmp(argumentString, pPtr->name, equalOffset) == 0
                  && *(pPtr->name+equalOffset) == '\0') {

                valueObj = Tcl_NewStringObj(valueInArgument+1, -1);
                /*fprintf(stderr, "... value from argument = %s\n", ObjStr(valueObj));*/
                NsfFlagObjSet(interp, argumentObj, paramPtr, serial,
                              pPtr, valueObj, NSF_FLAG_CONTAINS_VALUE);
                found = 1;
                break;
              }
            }
            if (found == 0) {
              Nsf_Param const *nextParamPtr = NextParam(currentParamPtr, lastParamPtr);

              if (nextParamPtr > lastParamPtr
                  || ((nextParamPtr->flags & NSF_ARG_NODASHALNUM) != 0u)) {
                Tcl_Obj *methodPathObj;

                methodPathObj = NsfMethodNamePath(interp, CallStackGetTclFrame(interp, NULL, 0),
                                                  NsfMethodName(procNameObj));
                return NsfUnexpectedNonposArgumentError(interp, argumentString,
                                                        (Nsf_Object *)object,
                                                        currentParamPtr, paramPtr,
                                                        methodPathObj);
              }
              pPtr = currentParamPtr = nextParamPtr;
            }
          } else {
            /*
             * Must be a classical nonpos arg; check for a matching parameter
             * definition.
             */
            int found = 0;

            assert(pPtr == currentParamPtr);

            if (likely(ch1 != '\0')) {
              if (unlikely(NsfParamDefsNonposLookup(interp, argumentString, currentParamPtr, &pPtr) != TCL_OK)) {
                return TCL_ERROR;
              } else {
                if (pPtr != NULL) {
                  found = 1;
                  NsfFlagObjSet(interp, argumentObj, paramPtr, serial, pPtr, NULL, 0);
                }
              }
            }

            /*
             * We might have found the argument starting with the dash in the
             * parameter definitions or not. If it was not found, then we can
             * advance to the next positional parameter and stuff the value in
             * there, if the parameter definition allows this.
             */
            if (found == 0) {
              int nonposArgError = 0;
              Nsf_Param const *nextParamPtr = NextParam(currentParamPtr, lastParamPtr);

              /*fprintf(stderr, "non-pos-arg '%s' not found, current %p %s last %p %s next %p %s\n",
                      argumentString,
                      currentParamPtr,  currentParamPtr->name,
                      lastParamPtr, lastParamPtr->name,
                      nextParamPtr, nextParamPtr->name);*/

              if (nextParamPtr > lastParamPtr) {
                nonposArgError = 1;
              } else if ((nextParamPtr->flags & NSF_ARG_NODASHALNUM) != 0u) {
                /*
                 * Check if argment is numeric, since we want to allow it as
                 * value even when NSF_ARG_NODASHALNUM was specified.
                 */
                nonposArgError = 1;

                if (argumentString[1] >= '0' && argumentString[1] <= '9') {
                  char *p;

                  (void)strtod(&argumentString[1], &p);
                  if (*p == '\0') {
                    /* argument is numeric */
                    nonposArgError = 0;
                  }
                }
              }

              if (nonposArgError != 0) {
                Tcl_Obj *methodPathObj = NsfMethodNamePath(interp,
                                                           CallStackGetTclFrame(interp, NULL, 0),
                                                           NsfMethodName(procNameObj));
                return NsfUnexpectedNonposArgumentError(interp, argumentString,
                                                        (Nsf_Object *)object,
                                                        currentParamPtr, paramPtr,
                                                        methodPathObj);
              }
              pPtr = currentParamPtr = nextParamPtr;
            }
          }
        }
        /* end of lookup loop */
      }
    }

    assert(pPtr != NULL);
    /*
     * pPtr points to the actual parameter (part of the currentParamPtr block)
     * or might point to a place past the last parameter, in which case an
     * unexpected argument was provided. o is the index of the actual
     * parameter, valueObj might be already provided for valueInArgument.
     */
    if (unlikely(pPtr > lastParamPtr)) {
      Tcl_Obj *methodPathObj;

      methodPathObj = NsfMethodNamePath(interp, CallStackGetTclFrame(interp, NULL, 0),
                                        NsfMethodName(procNameObj));
      /*fprintf(stderr, "call NsfUnexpectedArgumentError 2\n");*/
      return NsfUnexpectedArgumentError(interp, ObjStr(argumentObj),
                                        (Nsf_Object *)object, paramPtr,
                                        methodPathObj);
    }

    /*
     * Set the position in the downstream argv (normalized order)
     */
    j = pPtr - paramPtr;

#if defined(PARSE_TRACE_FULL)
    fprintf(stderr, "... pPtr->name %s/%d o %d objc %d\n", pPtr->name, pPtr->nrArgs, o, objc);
#endif
    if (*pPtr->name == '-') {
      /* process the nonpos arg */
      if (pPtr->nrArgs == 1) {
        /* The nonpos arg expects an argument */
        o++;
        if (unlikely(o >= objc)) {
          /* we expect an argument, but we are already at the end of the argument list */
          return NsfPrintError(interp, "value for parameter '%s' expected", pPtr->name);
        }
        assert(valueObj == NULL);
        valueObj = objv[o];
      } else {
        /* The nonpos arg expects no argument */
        if (valueObj == NULL) {
          valueObj = NsfGlobalObjs[NSF_ONE];
        }
      }

    } else if (unlikely((pPtr == lastParamPtr)
                        && (pPtr->converter == ConvertToNothing))) {
      /*
       * "args" was given, use the varargs interface.  Store the actual
       * argument into pcPtr->objv. No checking is performed on "args".
       */
      pcPtr->varArgs = 1;
      pcPtr->objv[j] = argumentObj;


#if defined(PARSE_TRACE_FULL)
      fprintf(stderr, "... args found o %d objc %d is dashdash %d [%d] <%s>\n", o, objc, dashdash, j, ObjStr(argumentObj));
#endif
      break;

    } else {
      /*
       * Process an ordinary positional argument.
       */
      currentParamPtr ++;

#if defined(PARSE_TRACE_FULL)
      fprintf(stderr, "... positional arg o %d objc %d, nrArgs %d next paramPtr %s\n",
              o, objc, pPtr->nrArgs, currentParamPtr->name);
#endif

      if (unlikely(pPtr->nrArgs == 0)) {
        /*
         * Allow positional arguments with 0 args for object parameter
         * aliases, which are always fired. Such parameter are non-consuming,
         * therefore the processing of the current argument is not finished, we
         * have to decrement o. We have to check here if we are already at the
         * end if the parameter vector.
         */
        o--;
        continue;
      }
      if (unlikely(dashdash)) {
        /* reset dashdash */
        dashdash = 0;
      }

      valueObj = argumentObj;
    }

#if defined(PARSE_TRACE_FULL)
    fprintf(stderr, "... setting parameter %s pos %d valueObj '%s'\n",
            pPtr->name, j,
            valueObj == argumentObj ? "=" : ObjStr(valueObj));
#endif

    /*
     * The value for the flag is now in the valueObj. We
     * check, whether it is value is permissible.
     */
    assert(valueObj != NULL);

    if (unlikely(ArgumentCheck(interp, valueObj, pPtr, processFlags,
                               &pcPtr->flags[j],
                               &pcPtr->clientData[j],
                               &pcPtr->objv[j]) != TCL_OK)) {
      if (pcPtr->flags[j] & NSF_PC_MUST_DECR) {pcPtr->status |= NSF_PC_STATUS_MUST_DECR;}
      return TCL_ERROR;
    }

    /*
     * Switches are more tricky: if the flag is provided without
     * valueInArgument, we take the default and invert it. If valueInArgument
     * was used, the default inversion must not happen.
     */
    if (likely(valueInArgument == NULL)) {
      if (unlikely(pPtr->converter == Nsf_ConvertToSwitch)) {
        /*fprintf(stderr,"... set INVERT_DEFAULT for '%s' flags %.6x\n",
          pPtr->name, pPtr->flags);*/
        pcPtr->flags[j] |= NSF_PC_INVERT_DEFAULT;
      }
    }

    /*fprintf(stderr, "... non-positional pcPtr %p check [%d] obj %p flags %.6x & %p\n",
      pcPtr, j, pcPtr->objv[j], pcPtr->flags[j], &(pcPtr->flags[j]));        */

    /*
     * Provide warnings for double-settings.
     */
    if (unlikely(pcPtr->flags[j] & NSF_ARG_SET)) {
      Tcl_Obj *cmdLineObj = Tcl_NewListObj(objc-1, objv+1);

      INCR_REF_COUNT(cmdLineObj);
      NsfLog(interp, NSF_LOG_WARN, "Non-positional parameter %s was passed more than once (%s%s%s %s)",
             pPtr->name, (object != NULL) ? ObjectName(object) : "", (object != NULL) ? " method " : "",
             ObjStr(procNameObj), ObjStr(cmdLineObj));
      DECR_REF_COUNT(cmdLineObj);
    }
    pcPtr->flags[j] |= NSF_ARG_SET;

    /*
     * Embed error message of converter in current context.
     */
    if (unlikely(pcPtr->flags[j] & NSF_ARG_WARN)) {
      Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
      Tcl_DString ds, *dsPtr = &ds;

      Tcl_DStringInit(dsPtr);
      INCR_REF_COUNT(resultObj);
      NsfDStringArgv(dsPtr, objc, objv);
      NsfLog(interp, NSF_LOG_WARN, "%s during:\n%s %s",
             ObjStr(resultObj), (object != NULL) ? ObjectName(object) : "nsf::proc", Tcl_DStringValue(dsPtr));
      DECR_REF_COUNT(resultObj);
      Tcl_DStringFree(dsPtr);
    }

    if (unlikely(pcPtr->flags[j] & NSF_PC_MUST_DECR)) {
      pcPtr->status |= NSF_PC_STATUS_MUST_DECR;
    }

    assert(pcPtr->varArgs == 0);

#if defined(PARSE_TRACE_FULL)
    fprintf(stderr, "... iterate on o %d objc %d, currentParamPtr %s\n",
            o, objc, currentParamPtr->name);
#endif
  }

  if ((currentParamPtr <= lastParamPtr) && (pcPtr->varArgs == 0)) {
    /* not all parameter processed, make sure varags is set */

    /*fprintf(stderr, ".... not all parms processed, pPtr '%s' j %ld nrParams %d last '%s' varArgs %d dashdash %d\n",
            currentParamPtr->name, currentParamPtr - paramPtr, nrParams, lastParamPtr->name,
            pcPtr->varArgs, dashdash);*/

    if (lastParamPtr->converter == ConvertToNothing) {
      pcPtr->varArgs = 1;
    }
  }

  /*
   * Set lastObjc as index of the first "unprocessed" parameter.
   */
  pcPtr->lastObjc = o;
  pcPtr->objc = nrParams;

  assert(ISOBJ(objv[pcPtr->lastObjc-1]));

#if defined(PARSE_TRACE_FULL)
  fprintf(stderr, "..... argv processed o %d lastObjc %d nrParams %d o<objc %d varargs %d\n",
          o, pcPtr->lastObjc, nrParams, o<objc, pcPtr->varArgs);
#endif

  return ArgumentDefaults(pcPtr, interp, paramPtr, nrParams, processFlags);
}

/***********************************
 * Begin result setting commands
 * (essentially List*() and support
 ***********************************/
/*
 *----------------------------------------------------------------------
 * ListVarKeys --
 *
 *    Return variable names of the provided hash table in the interp
 *    result. Optionally "pattern" might be used to filter the result list.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    Modifies interp result
 *
 *----------------------------------------------------------------------
 */
static int ListVarKeys(Tcl_Interp *interp, Tcl_HashTable *tablePtr, const char *pattern)
  nonnull(1);

static int
ListVarKeys(Tcl_Interp *interp, Tcl_HashTable *tablePtr, const char *pattern) {
  Tcl_HashEntry *hPtr;

  nonnull_assert(interp != NULL);

  if (pattern && NoMetaChars(pattern)) {
    Tcl_Obj *patternObj = Tcl_NewStringObj(pattern, -1);

    INCR_REF_COUNT(patternObj);
    hPtr = (tablePtr != NULL) ? Tcl_CreateHashEntry(tablePtr, (char *)patternObj, NULL) : NULL;
    if (hPtr != NULL) {
      Var *val = TclVarHashGetValue(hPtr);
      Tcl_SetObjResult(interp, TclVarHashGetKey(val));
    } else {
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_EMPTY]);
    }
    DECR_REF_COUNT(patternObj);

  } else {
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch hSrch;

    hPtr = (tablePtr != NULL) ? Tcl_FirstHashEntry(tablePtr, &hSrch) : NULL;
    for (; hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      Var *val = TclVarHashGetValue(hPtr);
      Tcl_Obj *key = TclVarHashGetKey(val);
      if (pattern == NULL || Tcl_StringMatch(ObjStr(key), pattern)) {
        Tcl_ListObjAppendElement(interp, list, key);
      }
    }
    Tcl_SetObjResult(interp, list);
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * GetOriginalCommand --
 *
 *    Obtain for an imported/aliased cmd the original definition.
 *
 * Results:
 *    Tcl command
 *
 * Side effects:
 *    none
 *
 *----------------------------------------------------------------------
 */
static Tcl_Command
GetOriginalCommand(Tcl_Command cmd)  /* The imported command for which the original
                                     * command should be returned.
                                     */
{
  Tcl_Command importedCmd;

  nonnull_assert(cmd != NULL);

  while (1) {
    /* dereference the namespace import reference chain */
    if ((importedCmd = TclGetOriginalCommand(cmd))) {
      cmd = importedCmd;
    }
    /* dereference the Next Scripting alias chain */
    if (Tcl_Command_deleteProc(cmd) == AliasCmdDeleteProc) {
      AliasCmdClientData *tcd = (AliasCmdClientData *)Tcl_Command_objClientData(cmd);
      cmd = tcd->aliasedCmd;
      continue;
    }
    break;
  }

  return cmd;
}

/*
 *----------------------------------------------------------------------
 * ListProcBody --
 *
 *    Return the body of a scripted proc as tcl interp result.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    Modifies interp result
 *
 *----------------------------------------------------------------------
 */
static int ListProcBody(Tcl_Interp *interp, Proc *procPtr, const char *methodName)
  nonnull(1) nonnull(2) nonnull(3);

static int
ListProcBody(Tcl_Interp *interp, Proc *procPtr, const char *methodName) {
  const char *body;

  nonnull_assert(interp != NULL);
  nonnull_assert(procPtr != NULL);
  nonnull_assert(methodName != NULL);

  body = ObjStr(procPtr->bodyPtr);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(StripBodyPrefix(body), -1));
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ListParamDefs --
 *
 *    Compute the parameter definition in one of four different forms.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    Modifies interp result
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *ListParamDefs(Tcl_Interp *interp, Nsf_Param const *paramsPtr,
                              NsfObject *contextObject, const char *pattern, NsfParamsPrintStyle style)
  nonnull(1) nonnull(2) returns_nonnull;

static Tcl_Obj *
ListParamDefs(Tcl_Interp *interp, Nsf_Param const *paramsPtr,
              NsfObject *contextObject,  const char *pattern,
              NsfParamsPrintStyle style) {
  Tcl_Obj *listObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramsPtr != NULL);

  switch (style) {
  case NSF_PARAMS_PARAMETER: listObj = ParamDefsFormat(interp, paramsPtr, contextObject, pattern); break;
  case NSF_PARAMS_LIST:      listObj = ParamDefsList(interp, paramsPtr, contextObject, pattern);   break;
  case NSF_PARAMS_NAMES:     listObj = ParamDefsNames(interp, paramsPtr, contextObject, pattern);  break;
  default: /* NSF_PARAMS_SYNTAX:*/ listObj = NsfParamDefsSyntax(interp, paramsPtr, contextObject, pattern); break;
  }

  return listObj;
}

/*
 *----------------------------------------------------------------------
 * ListCmdParams --
 *
 *    Obtains a cmd and a method name. As a side effect, sets the Tcl interp
 *    result to a list of parameter definitions, if available. The print-style
 *    NSF_PARAMS_NAMES, NSF_PARAMS_LIST, NSF_PARAMS_PARAMETER,
 *    NSF_PARAMS_SYNTAX controls the list content.
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    Sets interp result
 *
 *----------------------------------------------------------------------
 */

static int ListCmdParams(Tcl_Interp *interp, Tcl_Command cmd, NsfObject *contextObject,
                         const char *pattern, const char *methodName, NsfParamsPrintStyle printStyle)
  nonnull(1) nonnull(2) nonnull(5);

static int
ListCmdParams(Tcl_Interp *interp, Tcl_Command cmd,  NsfObject *contextObject,
                const char *pattern, const char *methodName, NsfParamsPrintStyle printStyle) {
  NsfParamDefs *paramDefs;
  Tcl_Obj *listObj;
  Proc *procPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmd != NULL);

  paramDefs = ParamDefsGet(cmd, NULL);

  if (paramDefs && paramDefs->paramsPtr) {
    /*
     * Obtain parameter info from paramDefs.
     */
    listObj = ListParamDefs(interp, paramDefs->paramsPtr, contextObject, pattern, printStyle);
    Tcl_SetObjResult(interp, listObj);
    DECR_REF_COUNT2("paramDefsObj", listObj);
    return TCL_OK;
  }

  procPtr = GetTclProcFromCommand(cmd);
  if (procPtr != NULL) {
    /*
     * Obtain parameter info from compiled locals.
     */
    CompiledLocal *args = procPtr->firstLocalPtr;

    listObj = Tcl_NewListObj(0, NULL);
    for ( ; args; args = args->nextPtr) {

      if (!TclIsCompiledLocalArgument(args)) {
        continue;
      }
      if (pattern && !Tcl_StringMatch(args->name, pattern)) {
        continue;
      }

      if (printStyle == NSF_PARAMS_SYNTAX && strcmp(args->name, "args") == 0) {
        if (args != procPtr->firstLocalPtr) {
          Tcl_AppendToObj(listObj, " ", 1);
        }
        Tcl_AppendToObj(listObj, "?/arg .../?", 11);
      } else {
        if (printStyle == NSF_PARAMS_SYNTAX) {
          /*
           * A default means that the argument is optional.
           */
          if (args->defValuePtr != NULL) {
            Tcl_AppendToObj(listObj, "?", 1);
            Tcl_AppendToObj(listObj, args->name, -1);
            Tcl_AppendToObj(listObj, "?", 1);
          } else {
            Tcl_AppendToObj(listObj, "/", 1);
            Tcl_AppendToObj(listObj, args->name, -1);
            Tcl_AppendToObj(listObj, "/", 1);
          }
          if (args->nextPtr != NULL) {
            Tcl_AppendToObj(listObj, " ", 1);
          }
        } else {
          Tcl_Obj *innerListObj = Tcl_NewListObj(0, NULL);

          Tcl_ListObjAppendElement(interp, innerListObj, Tcl_NewStringObj(args->name, -1));
          /*
           * Return default just for NSF_PARAMS_PARAMETER.
           */
          if ((args->defValuePtr != NULL) && (printStyle == NSF_PARAMS_PARAMETER)) {
            Tcl_ListObjAppendElement(interp, innerListObj, args->defValuePtr);
          }
          Tcl_ListObjAppendElement(interp, listObj, innerListObj);
        }
      }
    }

    Tcl_SetObjResult(interp, listObj);
    return TCL_OK;
  }

  {
    /*
     * If a command is not found for the object|class, check whether we
     * find the parameter definitions for the C-defined method.
     */
    Nsf_methodDefinition *mdPtr = Nsf_CmdDefinitionGet(((Command *)cmd)->objProc);
    if (mdPtr != NULL) {
        NsfParamDefs paramDefs = {mdPtr->paramDefs, mdPtr->nrParameters, 1, 0, NULL};
        Tcl_Obj *list = ListParamDefs(interp, paramDefs.paramsPtr, contextObject, pattern, printStyle);

        Tcl_SetObjResult(interp, list);
        DECR_REF_COUNT2("paramDefsObj", list);
        return TCL_OK;
    }
  }

  if (((Command *)cmd)->objProc == NsfSetterMethod) {
    SetterCmdClientData *cd = (SetterCmdClientData *)Tcl_Command_objClientData(cmd);

    if (cd && cd->paramsPtr) {
      Tcl_Obj *list;
      NsfParamDefs paramDefs;

      paramDefs.paramsPtr = cd->paramsPtr;
      paramDefs.nrParams = 1;
      list = ListParamDefs(interp, paramDefs.paramsPtr, contextObject, pattern, printStyle);
      Tcl_SetObjResult(interp, list);
      DECR_REF_COUNT2("paramDefsObj", list);
      return TCL_OK;
    } else {
      Tcl_SetObjResult(interp, Tcl_NewStringObj(methodName, -1));
      return TCL_OK;
    }
  }

  /*
   * In case, we failed so far to obtain a result, try to use the
   * object-system implementors definitions in the global array
   * ::nsf::parametersyntax. Note that we can only obtain the
   * parameter syntax this way.
   */
  if (printStyle == NSF_PARAMS_SYNTAX) {
    Tcl_DString ds, *dsPtr = &ds;
    Tcl_Obj *parameterSyntaxObj;

    Tcl_DStringInit(dsPtr);
    DStringAppendQualName(dsPtr, Tcl_Command_nsPtr(cmd), methodName);
    /*fprintf(stderr,"Looking up ::nsf::parametersyntax(%s) ...\n", Tcl_DStringValue(dsPtr));*/
    parameterSyntaxObj = Tcl_GetVar2Ex(interp, NsfGlobalStrings[NSF_ARRAY_PARAMETERSYNTAX],
                                       Tcl_DStringValue(dsPtr), TCL_GLOBAL_ONLY);

    /*fprintf(stderr, "No parametersyntax so far methodName %s cmd name %s ns %s\n",
      methodName, Tcl_GetCommandName(interp, cmd), Tcl_DStringValue(dsPtr));*/

    Tcl_DStringFree(dsPtr);
    if (parameterSyntaxObj != NULL) {
      Tcl_SetObjResult(interp, parameterSyntaxObj);
      return TCL_OK;
    }
  }

  if (Tcl_Command_objProc(cmd) == NsfForwardMethod) {
    return NsfPrintError(interp, "could not obtain parameter definition for forwarder '%s'",
                         methodName);
  } else if (CmdIsNsfObject(cmd)) {
    /* procPtr == NsfObjDispatch:

       reached for:
       ... ensemble objects
       ... plain objects
     */
    return TCL_OK;
  } else {
    /* reached e.g. for C-implemented Tcl command procs */
    return NsfPrintError(interp, "could not obtain parameter definition for method '%s'",
                         methodName);
  }
}


/*
 *----------------------------------------------------------------------
 * AppendForwardDefinition --
 *
 *    Append the parameters of a forward definition to the specified listObj.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Appending to listObj
 *
 *----------------------------------------------------------------------
 */
static void AppendForwardDefinition(Tcl_Interp *interp, Tcl_Obj *listObj, ForwardCmdClientData *tcd)
  nonnull(1) nonnull(2) nonnull(3);

static void
AppendForwardDefinition(Tcl_Interp *interp, Tcl_Obj *listObj, ForwardCmdClientData *tcd) {

  nonnull_assert(interp != NULL);
  nonnull_assert(listObj != NULL);
  nonnull_assert(tcd != NULL);

  if (tcd->prefix != NULL) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-prefix", -1));
    Tcl_ListObjAppendElement(interp, listObj, tcd->prefix);
  }
  if (tcd->subcommands != NULL) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-default", -1));
    Tcl_ListObjAppendElement(interp, listObj, tcd->subcommands);
  }
  if (tcd->objProc != NULL) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-earlybinding", -1));
  }
  if (tcd->frame == FrameObjectIdx) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-frame", 6));
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("object", 6));
  }
  Tcl_ListObjAppendElement(interp, listObj, tcd->cmdName);

  if (tcd->args != NULL) {
    Tcl_Obj **args;
    int nrArgs, i;

    Tcl_ListObjGetElements(interp, tcd->args, &nrArgs, &args);
    for (i = 0; i < nrArgs; i++) {
      Tcl_ListObjAppendElement(interp, listObj, args[i]);
    }
  }
}

/*
 *----------------------------------------------------------------------
 * AppendMethodRegistration --
 *
 *    Append to the listObj the command words needed for defintion /
 *    registration.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Appending to listObj
 *
 *----------------------------------------------------------------------
 */

static void AppendMethodRegistration(Tcl_Interp *interp, Tcl_Obj *listObj, const char *registerCmdName,
                                     NsfObject *object, const char *methodName, Tcl_Command cmd,
                                     int withObjFrame, int withPer_object, int withProtection)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4) nonnull(5) nonnull(6);

static void
AppendMethodRegistration(Tcl_Interp *interp, Tcl_Obj *listObj, const char *registerCmdName,
                         NsfObject *object, const char *methodName, Tcl_Command cmd,
                         int withObjFrame, int withPer_object, int withProtection) {

  nonnull_assert(interp != NULL);
  nonnull_assert(listObj != NULL);
  nonnull_assert(registerCmdName != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmd != NULL);

  Tcl_ListObjAppendElement(interp, listObj, object->cmdName);
  if (withProtection != 0) {
    Tcl_ListObjAppendElement(interp, listObj,
                             ((Tcl_Command_flags(cmd) & NSF_CMD_CALL_PRIVATE_METHOD) != 0)
                             ? Tcl_NewStringObj("private", 7)
                             : ((Tcl_Command_flags(cmd) & NSF_CMD_CALL_PROTECTED_METHOD) != 0)
                             ? Tcl_NewStringObj("protected", 9)
                             : Tcl_NewStringObj("public", 6));
  }

  if (!NsfObjectIsClass(object) || withPer_object) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("object", 6));
  }
  Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(registerCmdName, -1));
  Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(methodName, -1));

  if (withObjFrame != 0) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-frame", 6));
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("object", 6));
  }
  if (Tcl_Command_flags(cmd) & NSF_CMD_NONLEAF_METHOD) {
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-frame", 6));
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("method", 6));
  }
}

/*
 *----------------------------------------------------------------------
 * AppendReturnsClause --
 *
 *    Append to the listObj a returns clause, if it was spefified for the
 *    current cmd.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Appending to listObj
 *
 *----------------------------------------------------------------------
 */

static void AppendReturnsClause(Tcl_Interp *interp, Tcl_Obj *listObj, Tcl_Command cmd)
  nonnull(1) nonnull(2) nonnull(3);

static void
AppendReturnsClause(Tcl_Interp *interp, Tcl_Obj *listObj, Tcl_Command cmd) {
  NsfParamDefs *paramDefs;

  nonnull_assert(interp != NULL);
  nonnull_assert(listObj != NULL);
  nonnull_assert(cmd != NULL);

  paramDefs = ParamDefsGet(cmd, NULL);
  if (paramDefs && paramDefs->returns) {
    /* TODO: avoid hard-coding the script-level/NX-specific keyword "returns" */
    Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-returns", -1));
    Tcl_ListObjAppendElement(interp, listObj, paramDefs->returns);
  }
}

/*
 *----------------------------------------------------------------------
 * ListMethod --
 *
 *    Constuct a command to regenerate the specified method. The method might
 *    be scripted or not (alias, forwarder, ...). The command is returned in
 *    the interp result.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    Sets interp result
 *
 *----------------------------------------------------------------------
 */
static int ListMethod(Tcl_Interp *interp,
                      NsfObject *regObject,
                      NsfObject *defObject,
                      const char *methodName,
                      Tcl_Command cmd,
                      int subcmd,
                      NsfObject *contextObject,
                      const char *pattern,
                      int withPer_object)
  nonnull(1) nonnull(4) nonnull(5);

static int
ListMethod(Tcl_Interp *interp,
           NsfObject *regObject,
           NsfObject *defObject,
           const char *methodName,
           Tcl_Command cmd,
           int subcmd,
           NsfObject *contextObject,
           const char *pattern,
           int withPer_object) {

  Tcl_ObjCmdProc *procPtr;
  int outputPerObject;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmd != NULL);

  assert(*methodName != ':');

  Tcl_ResetResult(interp);

  if (regObject != NULL && !NsfObjectIsClass(regObject)) {
    withPer_object = 1;
    /* don't output "object" modifier, if regObject is not a class */
    outputPerObject = 0;
  } else {
    outputPerObject = withPer_object;
  }

  procPtr = Tcl_Command_objProc(cmd);

  switch (subcmd) {
  case InfomethodsubcmdRegistrationhandleIdx:
    {
      if (regObject != NULL) {
        Tcl_SetObjResult(interp, MethodHandleObj(regObject, withPer_object, methodName));
      }
      return TCL_OK;
    }
  case InfomethodsubcmdDefinitionhandleIdx:
    {
      if (defObject != NULL) {
        Tcl_SetObjResult(interp, MethodHandleObj(defObject,
                                                 NsfObjectIsClass(defObject) ? withPer_object : 1,
                                                 Tcl_GetCommandName(interp, cmd)));
      }
      return TCL_OK;
    }
  case InfomethodsubcmdExistsIdx:
    {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(CmdIsNsfObject(cmd) == 0));
      return TCL_OK;
    }
  case InfomethodsubcmdArgsIdx:
    {
      Tcl_Command importedCmd = GetOriginalCommand(cmd);
      return ListCmdParams(interp, importedCmd, contextObject, pattern, methodName, NSF_PARAMS_NAMES);
    }
  case InfomethodsubcmdParameterIdx:
    {
      Tcl_Command importedCmd = GetOriginalCommand(cmd);
      return ListCmdParams(interp, importedCmd, contextObject, pattern, methodName, NSF_PARAMS_PARAMETER);
    }
  case InfomethodsubcmdReturnsIdx:
    {
      Tcl_Command importedCmd;
      NsfParamDefs *paramDefs;

      importedCmd = GetOriginalCommand(cmd);
      paramDefs = ParamDefsGet(importedCmd, NULL);
      if (paramDefs && paramDefs->returns) {
        Tcl_SetObjResult(interp, paramDefs->returns);
      }
      return TCL_OK;
    }
  case InfomethodsubcmdSyntaxIdx:
    {
      Tcl_Command importedCmd = GetOriginalCommand(cmd);
      return ListCmdParams(interp, importedCmd, contextObject, pattern, methodName, NSF_PARAMS_SYNTAX);
    }

  case InfomethodsubcmdPreconditionIdx:
#if defined(NSF_WITH_ASSERTIONS)
      if (regObject != NULL) {
        NsfProcAssertion *procs = NULL;

        if (withPer_object == 1) {
          if (regObject->opt != NULL && regObject->opt->assertions != NULL) {
            procs = AssertionFindProcs(regObject->opt->assertions, methodName);
          }
        } else {
          NsfClass *class = (NsfClass *)regObject;
          if (class->opt != NULL && class->opt->assertions != NULL) {
            procs = AssertionFindProcs(class->opt->assertions, methodName);
          }
        }
        if (procs != NULL) {
          Tcl_SetObjResult(interp, AssertionList(interp, procs->pre));
        }
      }
#endif
      return TCL_OK;


  case InfomethodsubcmdPostconditionIdx:
#if defined(NSF_WITH_ASSERTIONS)
      if (regObject != NULL) {
        NsfProcAssertion *procs = NULL;

        if (withPer_object == 1) {
          if (regObject->opt != NULL && regObject->opt->assertions != NULL) {
            procs = AssertionFindProcs(regObject->opt->assertions, methodName);
          }
        } else {
          NsfClass *class = (NsfClass *)regObject;
          if (class->opt != NULL && class->opt->assertions != NULL) {
            procs = AssertionFindProcs(class->opt->assertions, methodName);
          }
        }
        if (procs != NULL) {
          Tcl_SetObjResult(interp, AssertionList(interp, procs->post));
        }
      }
#endif
      return TCL_OK;

  case InfomethodsubcmdSubmethodsIdx:
    {
      Tcl_Command origCmd = GetOriginalCommand(cmd);

      if (CmdIsNsfObject(origCmd)) {
        NsfObject *subObject = NsfGetObjectFromCmdPtr(origCmd);
        if (subObject != NULL) {
          return ListDefinedMethods(interp, subObject, NULL, 1 /* per-object */,
                                    NSF_METHODTYPE_ALL, CallprotectionAllIdx, 0);
        }
      }
      /* all other cases return empty */
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_EMPTY]);
      return TCL_OK;
    }
  }

  /*
   * The subcommands differ per type of method. The converter in
   * InfoMethods defines the types:
   *
   *    all|scripted|builtin|alias|forwarder|object|setter|nsfproc
   */
  if (GetTclProcFromCommand(cmd)) {
    /* a scripted method */
    switch (subcmd) {

    case InfomethodsubcmdTypeIdx:
      if (regObject != NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("scripted", -1));
      } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("proc", -1));
      }
      break;

    case InfomethodsubcmdBodyIdx:
      ListProcBody(interp, GetTclProcFromCommand(cmd), methodName);
      break;

    case InfomethodsubcmdDefinitionIdx:
      {
        resultObj = Tcl_NewListObj(0, NULL);
        /* todo: don't hard-code registering command name "method" / NSF_METHOD */
        if (regObject != NULL) {
          AppendMethodRegistration(interp, resultObj, NsfGlobalStrings[NSF_METHOD],
                                   regObject, methodName, cmd, 0, outputPerObject, 1);
        } else {
          Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("::proc", -1));
          Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(methodName, -1));
        }
        ListCmdParams(interp, cmd, contextObject, NULL, methodName, NSF_PARAMS_PARAMETER);
        Tcl_ListObjAppendElement(interp, resultObj, Tcl_GetObjResult(interp));

        AppendReturnsClause(interp, resultObj, cmd);

        ListProcBody(interp, GetTclProcFromCommand(cmd), methodName);
        Tcl_ListObjAppendElement(interp, resultObj, Tcl_GetObjResult(interp));

#if defined(NSF_WITH_ASSERTIONS)
        if (regObject != NULL) {
          NsfAssertionStore *assertions;

          if (withPer_object == 1) {
            assertions = (regObject->opt != NULL) ? regObject->opt->assertions : NULL;
          } else {
            NsfClass *class = (NsfClass *)regObject;
            assertions = (class->opt != NULL) ? class->opt->assertions : NULL;
          }

          if (assertions != NULL) {
            NsfProcAssertion *procs = AssertionFindProcs(assertions, methodName);
            if (procs != NULL) {
              Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("-precondition", -1));
              Tcl_ListObjAppendElement(interp, resultObj, AssertionList(interp, procs->pre));
              Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("-postcondition", -1));
              Tcl_ListObjAppendElement(interp, resultObj, AssertionList(interp, procs->post));
            }
          }
        }
#endif
        Tcl_SetObjResult(interp, resultObj);
        break;
      }
    }

  } else if (procPtr == NsfForwardMethod) {
    /* forwarder */
    switch (subcmd) {
    case InfomethodsubcmdTypeIdx:
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_FORWARD]);
      break;
    case InfomethodsubcmdDefinitionIdx:
      if (regObject != NULL) {
        ClientData clientData;

        assert(cmd != NULL);

        clientData = Tcl_Command_objClientData(cmd);
        if (clientData != NULL) {
          resultObj = Tcl_NewListObj(0, NULL);
          /* todo: don't hard-code registering command name "forward" / NSF_FORWARD*/
          AppendMethodRegistration(interp, resultObj, NsfGlobalStrings[NSF_FORWARD],
                                   regObject, methodName, cmd, 0, outputPerObject, 1);
          AppendReturnsClause(interp, resultObj, cmd);

          AppendForwardDefinition(interp, resultObj, clientData);
          Tcl_SetObjResult(interp, resultObj);
        }
      }
      break;
    }

  } else if (procPtr == NsfSetterMethod) {
    /* setter methods */
    switch (subcmd) {
    case InfomethodsubcmdTypeIdx:
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_SETTER]);
      break;
    case InfomethodsubcmdDefinitionIdx:
      if (regObject != NULL) {
        SetterCmdClientData *cd = (SetterCmdClientData *)Tcl_Command_objClientData(cmd);

        resultObj = Tcl_NewListObj(0, NULL);
        /* todo: don't hard-code registering command name "setter" / NSF_SETTER */
        AppendMethodRegistration(interp, resultObj, NsfGlobalStrings[NSF_SETTER], regObject,
                                 (cd && cd->paramsPtr) ? ObjStr(cd->paramsPtr->paramObj) : methodName,
                                 cmd, 0, outputPerObject, 1);
        Tcl_SetObjResult(interp, resultObj);
      }
      break;
    }
  } else if (procPtr == NsfProcStub) {
    /*
     * Special nsfproc handling:
     */
    NsfProcClientData *tcd = Tcl_Command_objClientData(cmd);

    if (tcd && tcd->procName) {
      Tcl_Command procCmd = Tcl_GetCommandFromObj(interp, tcd->procName);
      Tcl_DString ds, *dsPtr = &ds;
      Tcl_Obj *resultObj;

      switch (subcmd) {

      case InfomethodsubcmdTypeIdx:
        Tcl_SetObjResult(interp, Tcl_NewStringObj("nsfproc", -1));
        break;

      case InfomethodsubcmdBodyIdx:
        ListProcBody(interp, GetTclProcFromCommand(procCmd), methodName);
        break;

      case InfomethodsubcmdDefinitionIdx:
        resultObj = Tcl_NewListObj(0, NULL);
        Tcl_DStringInit(dsPtr);
        DStringAppendQualName(dsPtr, Tcl_Command_nsPtr(cmd), methodName);
        /* don't hardcode names */
        Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("::nsf::proc", -1));
        if (tcd->with_ad != 0) {
          Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj("-ad", 3));
        }
        Tcl_ListObjAppendElement(interp, resultObj,
                                 Tcl_NewStringObj(Tcl_DStringValue(dsPtr),
                                                  Tcl_DStringLength(dsPtr)));
        ListCmdParams(interp, cmd, NULL, NULL, Tcl_DStringValue(dsPtr), NSF_PARAMS_PARAMETER);
        Tcl_ListObjAppendElement(interp, resultObj, Tcl_GetObjResult(interp));
        ListProcBody(interp, GetTclProcFromCommand(procCmd), methodName);
        Tcl_ListObjAppendElement(interp, resultObj, Tcl_GetObjResult(interp));
        Tcl_SetObjResult(interp, resultObj);
        Tcl_DStringFree(dsPtr);
        break;
      }
    }

  } else if (defObject != NULL) {
    /*
     * The cmd must be an alias or object.
     *
     * Note that some aliases come with procPtr == NsfObjDispatch.
     * In order to distinguish between "object" and alias, we have
     * to do the lookup for the entryObj to determine wether it is
     * really an alias.
     */
    Tcl_Obj *entryObj;

    entryObj = AliasGet(interp, defObject->cmdName,
                        Tcl_GetCommandName(interp, cmd),
                        regObject != defObject ? 1 : withPer_object, 0);
    /*
      fprintf(stderr, "aliasGet %s -> %s/%s (%d) returned %p\n",
      ObjectName(defObject), methodName, Tcl_GetCommandName(interp, cmd),
      withPer_object, entryObj);
      fprintf(stderr, "... regObject %p %s\n", regObject, ObjectName(regObject));
      fprintf(stderr, "... defObject %p %s\n", defObject, ObjectName(defObject));
    */

    if (entryObj != NULL) {
      /* is an alias */
      switch (subcmd) {
      case InfomethodsubcmdTypeIdx:
        Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_ALIAS]);
        break;
      case InfomethodsubcmdDefinitionIdx:
        if (regObject != NULL) {
          int nrElements;
          Tcl_Obj **listElements;

          resultObj = Tcl_NewListObj(0, NULL);
          Tcl_ListObjGetElements(interp, entryObj, &nrElements, &listElements);
          /* todo: don't hard-code registering command name "alias" / NSF_ALIAS */
          AppendMethodRegistration(interp, resultObj, NsfGlobalStrings[NSF_ALIAS],
                                   regObject, methodName, cmd,
                                   procPtr == NsfObjscopedMethod,
                                   outputPerObject, 1);
          AppendReturnsClause(interp, resultObj, cmd);
          Tcl_ListObjAppendElement(interp, resultObj, listElements[nrElements-1]);
          Tcl_SetObjResult(interp, resultObj);
        }
        break;

      case InfomethodsubcmdOriginIdx:
        {
          int nrElements;
          Tcl_Obj **listElements;
          Tcl_ListObjGetElements(interp, entryObj, &nrElements, &listElements);
          Tcl_SetObjResult(interp, listElements[nrElements-1]);
          break;
        }
      }
    } else {
      /* check, to be on the safe side */
      if (CmdIsNsfObject(cmd)) {
        /* the command is an object */
        switch (subcmd) {
        case InfomethodsubcmdTypeIdx:
          Tcl_SetObjResult(interp, Tcl_NewStringObj("object", -1));
          break;
        case InfomethodsubcmdDefinitionIdx:
          {
            NsfObject *subObject = NsfGetObjectFromCmdPtr(cmd);

            assert(subObject != NULL);
            resultObj = Tcl_NewListObj(0, NULL);
            AppendMethodRegistration(interp, resultObj, "create",
                                     &(subObject->cl)->object,
                                     ObjStr(subObject->cmdName), cmd, 0, 0, 0);
            Tcl_SetObjResult(interp, resultObj);
            break;
          }
        }
      } else {
        /*
         * Should never happen.
         *
         * The warning is just a guess, so we don't raise an error here.
         */
        NsfLog(interp, NSF_LOG_WARN, "Could not obtain alias definition for %s. "
               "Maybe someone deleted the alias %s for object %s?",
               methodName, methodName, ObjectName(regObject));
        Tcl_ResetResult(interp);
      }
    }
  } else {
    /*
     * The cmd must be a plain unregisted cmd
     */

    switch (subcmd) {
    case InfomethodsubcmdTypeIdx:
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_CMD]);
      break;
    case InfomethodsubcmdDefinitionIdx:
      break;
    case InfomethodsubcmdOriginIdx:
      break;
    }
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * ListMethodResolve --
 *
 *    Call essentially ListMethod(), but try to resolve the method name/handle
 *    first.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int
ListMethodResolve(Tcl_Interp *interp, int subcmd,
                  NsfObject *contextObject, const char *pattern,
                  Tcl_Namespace *nsPtr, NsfObject *object,
                  Tcl_Obj *methodNameObj, int fromClassNS)
  nonnull(1) nonnull(7);

static int
ListMethodResolve(Tcl_Interp *interp, int subcmd,
                  NsfObject *contextObject, const char *pattern,
                  Tcl_Namespace *nsPtr, NsfObject *object,
                  Tcl_Obj *methodNameObj, int fromClassNS) {
  NsfObject *regObject, *defObject;
  const char *methodName1 = NULL;
  int result = TCL_OK;
  Tcl_DString ds, *dsPtr = &ds;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodNameObj != NULL);

  Tcl_DStringInit(dsPtr);

  cmd = ResolveMethodName(interp, nsPtr, methodNameObj,
                          dsPtr, &regObject, &defObject, &methodName1, &fromClassNS);
  /*
   * If the cmd is not found, we return for every sub-command but "exists"
   * empty.
   */
  if (likely(cmd != NULL)) {

    result = ListMethod(interp, (regObject != NULL) ? regObject : object, (defObject != NULL) ? defObject : object,
                        methodName1, cmd, subcmd,
                        contextObject, pattern, (fromClassNS != 0) ? 0 : 1);

  } else if (subcmd == InfomethodsubcmdExistsIdx) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
  }

  Tcl_DStringFree(dsPtr);
  return result;
}


/*
 *----------------------------------------------------------------------
 * MethodSourceMatches --
 *
 *    Check, whether the provided class or object (mutually exclusive) matches
 *    with the required method source (typically all|application|system).
 *
 * Results:
 *    Returns true or false
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int MethodSourceMatches(int withSource, NsfClass *cl, NsfObject *object) {
  int isBaseClass;

  if (withSource == SourceAllIdx) {
    return 1;
  }

  if (cl == NULL) {
    /*
     * If the method is object specific, it can't be from a baseclass and must
     * be application specific.
     */
    assert(object != NULL);
    return (withSource == SourceApplicationIdx && !IsBaseClass(object));
  }
  assert(cl != NULL);

  isBaseClass = IsBaseClass(&cl->object);
  if (withSource == SourceSystemIdx && isBaseClass) {
    return 1;
  } else if (withSource == SourceApplicationIdx && !isBaseClass) {
    return 1;
  }
  return 0;
}


/*
 *----------------------------------------------------------------------
 * MethodTypeMatches --
 *
 *    Check, whether the provided method (specified as a cmd) matches with the
 *    required method type (typically
 *    all|scripted|builtin|alias|forwarder|object|setter).
 *
 * Results:
 *    Returns true or false
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int MethodTypeMatches(Tcl_Interp *interp, int methodType, Tcl_Command cmd,
                             NsfObject *object, const char *methodName, int withPer_object,
                             int *isObject)
  nonnull(1) nonnull(3) nonnull(5) nonnull(7);

static int
MethodTypeMatches(Tcl_Interp *interp, int methodType, Tcl_Command cmd,
                  NsfObject *object, const char *methodName, int withPer_object,
                  int *isObject) {
  Tcl_ObjCmdProc *proc, *resolvedProc;
  Tcl_Command importedCmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(isObject != NULL);

  proc = Tcl_Command_objProc(cmd);
  importedCmd = GetOriginalCommand(cmd);
  resolvedProc = Tcl_Command_objProc(importedCmd);

  /*
   * Return always state isObject, since the cmd might be an ensemble,
   * where we have to search further
   */
  *isObject = CmdIsNsfObject(importedCmd);

  if (methodType == NSF_METHODTYPE_ALIAS) {
    if (!(proc == NsfProcAliasMethod || AliasGet(interp, object->cmdName, methodName, withPer_object, 0))) {
      return 0;
    }
  } else {
    if (proc == NsfProcAliasMethod) {
      if ((methodType & NSF_METHODTYPE_ALIAS) == 0) {
        return 0;
      }
    }
    /* the following cases are disjoint */
    if (CmdIsProc(importedCmd)) {
      /*fprintf(stderr,"%s scripted %d\n", methodName, methodType & NSF_METHODTYPE_SCRIPTED);*/
      if ((methodType & NSF_METHODTYPE_SCRIPTED) == 0) {
        return 0;
      }
    } else if (resolvedProc == NsfForwardMethod) {
      if ((methodType & NSF_METHODTYPE_FORWARDER) == 0) {
        return 0;
      }
    } else if (resolvedProc == NsfSetterMethod) {
      if ((methodType & NSF_METHODTYPE_SETTER) == 0) {
        return 0;
      }
    } else if (*isObject) {
      if ((methodType & NSF_METHODTYPE_OBJECT) == 0) {
        return 0;
      }
    } else if (resolvedProc == NsfProcStub) {
      if ((methodType & NSF_METHODTYPE_NSFPROC) == 0) {
        return 0;
      }
    } else if ((methodType & NSF_METHODTYPE_OTHER) == 0) {
      /* fprintf(stderr,"OTHER %s not wanted %.4x\n", methodName, methodType);*/
      return 0;
    }
    /* NsfObjscopedMethod ??? */
  }
  return 1;
}

/*
 *----------------------------------------------------------------------
 * ProtectionMatches --
 *
 *    Check, whether the provided method (specified as a cmd) matches with the
 *    required call-protection (typically all|public|protected|private).
 *
 * Results:
 *    Returns true or false
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int ProtectionMatches(int withCallprotection, Tcl_Command cmd) nonnull(2);

static int
ProtectionMatches(int withCallprotection, Tcl_Command cmd) {
  int result, isProtected, isPrivate, cmdFlags;

  nonnull_assert(cmd != NULL);

  cmdFlags = Tcl_Command_flags(cmd);
  isProtected = (cmdFlags & NSF_CMD_CALL_PROTECTED_METHOD) != 0u;
  isPrivate = (cmdFlags & NSF_CMD_CALL_PRIVATE_METHOD) != 0u;

  if (withCallprotection == CallprotectionNULL) {
    withCallprotection = CallprotectionPublicIdx;
  }
  switch (withCallprotection) {
  case CallprotectionAllIdx: result = 1; break;
  case CallprotectionPublicIdx: result = (isProtected == 0); break;
  case CallprotectionProtectedIdx: result = isProtected && !isPrivate; break;
  case CallprotectionPrivateIdx: result = isPrivate; break;
  default: result = 1;
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ListMethodKeys --
 *
 *      List the method names contained in the specified hash-table
 *      according to the filtering options (types, pattern,
 *      protection, etc.). Optionally, a name prefix can be provided
 *      in form of a Tcl_DString. The result is placed into the interp
 *      result.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Setting interp result.
 *
 *----------------------------------------------------------------------
 */
static int ListMethodKeys(Tcl_Interp *interp, Tcl_HashTable *tablePtr,
                          Tcl_DString *prefix, const char *pattern,
                          int methodType, int withCallprotection, int withPath,
                          Tcl_HashTable *dups, NsfObject *object, int withPer_object)
  nonnull(1) nonnull(2);

static int
ListMethodKeys(Tcl_Interp *interp, Tcl_HashTable *tablePtr,
               Tcl_DString *prefix, const char *pattern,
               int methodType, int withCallprotection, int withPath,
               Tcl_HashTable *dups, NsfObject *object, int withPer_object) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;
  Tcl_Command cmd;
  const char *key;
  int isObject, methodTypeMatch;
  int prefixLength = (prefix != NULL) ? Tcl_DStringLength(prefix) : 0;
  Tcl_Obj *resultObj = Tcl_GetObjResult(interp);

  nonnull_assert(interp != NULL);
  nonnull_assert(tablePtr != NULL);

  if (pattern && NoMetaChars(pattern) && strchr(pattern, ' ') == NULL) {
    /*
     * We have a pattern that can be used for direct lookup; no need
     * to iterate.
     */
    hPtr = Tcl_CreateHashEntry(tablePtr, pattern, NULL);
    if (hPtr != NULL) {
      NsfObject *childObject;
      Tcl_Command origCmd;

      key = Tcl_GetHashKey(tablePtr, hPtr);
      cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
      methodTypeMatch = MethodTypeMatches(interp, methodType, cmd, object, key,
                                          withPer_object, &isObject);

      if (Tcl_Command_flags(cmd) & NSF_CMD_CLASS_ONLY_METHOD && !NsfObjectIsClass(object)) {
        return TCL_OK;
      }
      /*
       * Aliased objects methods return 1 but lookup from cmd returns
       * NULL. Below, we are just interested on true subobjects.
       */
      origCmd = GetOriginalCommand(cmd);
      childObject = (isObject == 1) ? NsfGetObjectFromCmdPtr(origCmd) : NULL;

      if (childObject != NULL) {

        if (withPath != 0) {
          return TCL_OK;
        }

        /*
         * Treat aliased object dispatch different from direct object
         * dispatches.
         */
#if 0
        if (cmd == origCmd && (childObject->flags & NSF_ALLOW_METHOD_DISPATCH ) == 0u) {
          /*fprintf(stderr, "no method dispatch allowed on child %s\n", ObjectName(childObject));*/
          return TCL_OK;
        }
#endif
      }

      if (ProtectionMatches(withCallprotection, cmd) && methodTypeMatch) {
        if (prefixLength != 0) {
          Tcl_DStringAppend(prefix, key, -1);
          key = Tcl_DStringValue(prefix);
        }
        if (dups != NULL) {
          int new;
          Tcl_CreateHashEntry(dups, key, &new);
          if (new != 0) {
            Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(key, -1));
          }
        } else {
          Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(key, -1));
        }
      }
    }
    return TCL_OK;

  } else {

    /*
     * We have to iterate over the elements
     */
    for (hPtr = Tcl_FirstHashEntry(tablePtr, &hSrch);
         hPtr;
         hPtr = Tcl_NextHashEntry(&hSrch)) {
      NsfObject *childObject;
      Tcl_Command origCmd;

      key = Tcl_GetHashKey(tablePtr, hPtr);
      cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
      if (prefixLength != 0) {Tcl_DStringTrunc(prefix, prefixLength);}
      methodTypeMatch = MethodTypeMatches(interp, methodType, cmd, object, key,
                                          withPer_object, &isObject);
      /*
       * Aliased objects methods return 1 but lookup from cmd returns
       * NULL. Below, we are just interested on true subobjects.
       */
      origCmd = GetOriginalCommand(cmd);
      childObject = (isObject == 1) ? NsfGetObjectFromCmdPtr(origCmd) : NULL;

      if (childObject != NULL) {
        if (withPath != 0) {
          Tcl_DString ds, *dsPtr = &ds;
          Tcl_HashTable *cmdTablePtr = (childObject->nsPtr != NULL) ? Tcl_Namespace_cmdTablePtr(childObject->nsPtr) : NULL;

          if (cmdTablePtr == NULL) {
            /* nothing to do */
            continue;
          }
          if ((childObject->flags & NSF_IS_SLOT_CONTAINER) != 0u) {
            /* Don't report slot container */
            continue;
          }
          if ((childObject->flags & NSF_KEEP_CALLER_SELF) == 0u) {
            /* Do only report sub-objects with keep caller self */
            continue;
          }

          /*fprintf(stderr, "ListMethodKeys key %s append key space flags %.6x\n",
            key, childObject->flags);*/
          if (prefix == NULL) {
            DSTRING_INIT(dsPtr);
            Tcl_DStringAppend(dsPtr, key, -1);
            Tcl_DStringAppend(dsPtr, " ", 1);

            ListMethodKeys(interp, cmdTablePtr, dsPtr, pattern, methodType, withCallprotection,
                           1, dups, object, withPer_object);
            DSTRING_FREE(dsPtr);
          } else {
            Tcl_DStringAppend(prefix, key, -1);
            Tcl_DStringAppend(prefix, " ", 1);
            ListMethodKeys(interp, cmdTablePtr, prefix, pattern, methodType, withCallprotection,
                           1, dups, object, withPer_object);
          }
          /* don't list ensembles by themselves */
          continue;
        }

        /*
         * Treat aliased object dispatch different from direct object
         * dispatches.
         */
#if 0
        if (cmd == origCmd && (childObject->flags & NSF_ALLOW_METHOD_DISPATCH ) == 0u) {
          /*fprintf(stderr, "no method dispatch allowed on child %s\n", ObjectName(childObject));*/
          continue;
        }
#endif

      }

      if (Tcl_Command_flags(cmd) & NSF_CMD_CLASS_ONLY_METHOD && !NsfObjectIsClass(object)) {
        continue;
      }
      if (!ProtectionMatches(withCallprotection, cmd)
          || !methodTypeMatch
          ) {
        continue;
      }

      if (prefixLength != 0) {
        Tcl_DStringAppend(prefix, key, -1);
        key = Tcl_DStringValue(prefix);
      }

      if (pattern && !Tcl_StringMatch(key, pattern)) {
        continue;
      }
      if (dups != NULL) {
        int new;
        Tcl_CreateHashEntry(dups, key, &new);
        if (new == 0) {
          continue;
        }
      }
      Tcl_ListObjAppendElement(interp, resultObj, Tcl_NewStringObj(key, -1));
    }
  }
  /*fprintf(stderr, "listkeys returns '%s'\n", ObjStr(Tcl_GetObjResult(interp)));*/
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ListChildren --
 *
 *      List the children of the specified object. The result can be filtered
 *      via a pattern or a type.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Setting interp result.
 *
 *----------------------------------------------------------------------
 */
static int ListChildren(Tcl_Interp *interp, NsfObject *object, const char *pattern,
                        int classesOnly, NsfClass *type)
  nonnull(1) nonnull(2);

static int
ListChildren(Tcl_Interp *interp, NsfObject *object, const char *pattern,
             int classesOnly, NsfClass *type) {
  NsfObject *childObject;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->nsPtr == NULL) {
    return TCL_OK;
  }

  if (pattern != NULL && NoMetaChars(pattern)) {
    Tcl_DString ds, *dsPtr = &ds;
    Tcl_DStringInit(dsPtr);

    if (*pattern != ':') {
      /* build a fully qualified name */
      DStringAppendQualName(dsPtr, object->nsPtr, pattern);
      pattern = Tcl_DStringValue(dsPtr);
    }

    if ((childObject = GetObjectFromString(interp, pattern)) &&
        (!classesOnly || NsfObjectIsClass(childObject)) &&
        (!type || IsSubType(childObject->cl, type)) &&
        (Tcl_Command_nsPtr(childObject->id) == object->nsPtr)  /* true children */
        ) {
      Tcl_SetObjResult(interp, childObject->cmdName);
    } else {
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_EMPTY]);
    }
    Tcl_DStringFree(dsPtr);

  } else {
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch hSrch;
    Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(object->nsPtr);
    Tcl_HashEntry *hPtr;

    for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch);
         hPtr;
         hPtr = Tcl_NextHashEntry(&hSrch)) {
      const char *key = Tcl_GetHashKey(cmdTablePtr, hPtr);

      if (pattern == NULL || Tcl_StringMatch(key, pattern)) {
        Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);

        /*fprintf(stderr, "... check %s child key %s child object %p %p\n",
                ObjectName(object), key, GetObjectFromString(interp, key),
                NsfGetObjectFromCmdPtr(cmd));*/

        if ((childObject = NsfGetObjectFromCmdPtr(cmd)) &&
            (!classesOnly || NsfObjectIsClass(childObject)) &&
            (!type || IsSubType(childObject->cl, type)) &&
            (Tcl_Command_nsPtr(childObject->id) == object->nsPtr)  /* true children */
            ) {
          Tcl_ListObjAppendElement(interp, list, childObject->cmdName);
        }
      }
    }
    Tcl_SetObjResult(interp, list);
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ListForward --
 *
 *      List registered forwareder defined in the hash table. The result can be filtered
 *      via a pattern, optionally the forward definition is returned.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Setting interp result.
 *
 *----------------------------------------------------------------------
 */
static int ListForward(Tcl_Interp *interp, Tcl_HashTable *tablePtr,
            const char *pattern, int withDefinition)
  nonnull(1) nonnull(2);

static int
ListForward(Tcl_Interp *interp, Tcl_HashTable *tablePtr,
            const char *pattern, int withDefinition) {

  nonnull_assert(interp != NULL);
  nonnull_assert(tablePtr != NULL);

  if (withDefinition != 0) {
    Tcl_HashEntry *hPtr = (pattern != NULL) ? Tcl_CreateHashEntry(tablePtr, pattern, NULL) : NULL;
    /*
     * Notice: we don't use pattern for wildcard matching here; pattern can
     * only contain wildcards when used without "-definition".
     */
    if (hPtr != NULL) {
      Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
      ClientData clientData = (cmd != NULL) ? Tcl_Command_objClientData(cmd) : NULL;
      ForwardCmdClientData *tcd = (ForwardCmdClientData *)clientData;

      if (tcd != NULL && Tcl_Command_objProc(cmd) == NsfForwardMethod) {
        Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);

        AppendForwardDefinition(interp, listObj, tcd);
        Tcl_SetObjResult(interp, listObj);
        return TCL_OK;
      }
    }
    return NsfPrintError(interp, "'%s' is not a forwarder", pattern);
  }
  return ListMethodKeys(interp, tablePtr, NULL, pattern, NSF_METHODTYPE_FORWARDER,
                        CallprotectionAllIdx, 0, NULL, NULL, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * ListDefinedMethods --
 *
 *      List the methods defined by the specified object/class
 *      according to the filtering options (types, pattern,
 *      protection, etc.). The result is placed into the interp
 *      result.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Setting interp result.
 *
 *----------------------------------------------------------------------
 */
static int
ListDefinedMethods(Tcl_Interp *interp, NsfObject *object, const char *pattern,
                   int withPer_object, int methodType, int withCallproctection,
                   int withPath) {
  Tcl_HashTable *cmdTablePtr;
  Tcl_DString ds, *dsPtr = NULL;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (pattern && *pattern == ':' && *(pattern + 1) == ':') {
    Namespace *nsPtr, *dummy1Ptr, *dummy2Ptr;
    const char *remainder;

    /*fprintf(stderr, "we have a colon pattern '%s' methodtype %.6x\n", pattern, methodType);*/

    TclGetNamespaceForQualName(interp, pattern, NULL, 0,
                               &nsPtr, &dummy1Ptr, &dummy2Ptr, &remainder);
    /*fprintf(stderr,
            "TclGetNamespaceForQualName with %s => (%p %s) (%p %s) (%p %s) (%p %s)\n",
            pattern,
            nsPtr, (nsPtr != NULL) ? nsPtr->fullName : "",
            dummy1Ptr, (dummy1Ptr != NULL) ? dummy1Ptr->fullName : "",
            dummy2Ptr, (dummy2Ptr != NULL) ? dummy2Ptr->fullName : "",
            remainder, (remainder != 0) ? remainder : "");*/
    if (nsPtr != NULL) {
      cmdTablePtr = Tcl_Namespace_cmdTablePtr(nsPtr);
      dsPtr = &ds;
      Tcl_DStringInit(dsPtr);
      Tcl_DStringAppend(dsPtr, nsPtr->fullName, -1);
      if (Tcl_DStringLength(dsPtr) > 2) {
        Tcl_DStringAppend(dsPtr, "::", 2);
      }
      pattern = remainder;
    } else {
      cmdTablePtr = NULL;
    }
  } else if (NsfObjectIsClass(object) && !withPer_object) {
    cmdTablePtr = Tcl_Namespace_cmdTablePtr(((NsfClass *)object)->nsPtr);
  } else {
    cmdTablePtr = (object->nsPtr != NULL) ? Tcl_Namespace_cmdTablePtr(object->nsPtr) : NULL;
  }

  if (cmdTablePtr != NULL) {
    ListMethodKeys(interp, cmdTablePtr, dsPtr, pattern, methodType, withCallproctection, withPath,
                   NULL, object, withPer_object);
    if (dsPtr != NULL) {
      Tcl_DStringFree(dsPtr);
    }
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ListSuperClasses --
 *
 *      List the superclasses of a class. Optionally the transitive closure is
 *      computed and the result can be filtered via a pattern.
 *
 * Results:
 *      Tcl result code.
 *
 * Side effects:
 *      Setting interp result.
 *
 *----------------------------------------------------------------------
 */

static int ListSuperClasses(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *pattern, int withClosure)
  nonnull(1) nonnull(2);

static int
ListSuperClasses(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *pattern, int withClosure) {
  NsfObject *matchObject = NULL;
  Tcl_Obj *patternObj = NULL, *outObjPtr;
  const char *patternString = NULL;
  ClientData clientData;
  int rc;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  if (cl->super == NULL) {
    return TCL_OK;
  }

  if (pattern != NULL &&
      ConvertToObjpattern(interp, pattern, NULL, &clientData, &outObjPtr) == TCL_OK) {
    patternObj = (Tcl_Obj *)clientData;
    if (GetMatchObject(interp, patternObj, pattern, &matchObject, &patternString) == -1) {
      /*
       * The pattern has no meta chars and does not correspond to an existing
       * object. Therefore, it can't be a superclass.
       */
      if (patternObj != NULL) {
        DECR_REF_COUNT2("patternObj", patternObj);
      }
      return TCL_OK;
    }
  }

  if (withClosure != 0) {
    NsfClasses *pl = PrecedenceOrder(cl);
    if (pl != NULL) {
      pl = pl->nextPtr;
    }
    rc = AppendMatchingElementsFromClasses(interp, pl, patternString, matchObject);
  } else {
    NsfClasses *clSuper = NsfReverseClasses(cl->super);
    rc = AppendMatchingElementsFromClasses(interp, clSuper, patternString, matchObject);
    NsfClassListFree(clSuper);
  }

  if (matchObject != NULL) {
    Tcl_SetObjResult(interp, (rc != 0) ? matchObject->cmdName : NsfGlobalObjs[NSF_EMPTY]);
  }

  if (patternObj != NULL) {
    DECR_REF_COUNT2("patternObj", patternObj);
  }
  return TCL_OK;
}


/********************************
 * End result setting commands
 ********************************/

/*
 *----------------------------------------------------------------------
 *
 * AliasIndex --
 *
 *      The alias index is an internal data structure to keep track how
 *      aliases are constructed. This function computes the key of the index.
 *
 * Results:
 *      string value of the index
 *
 * Side effects:
 *      updating DString
 *
 *----------------------------------------------------------------------
 */
static const char *AliasIndex(Tcl_DString *dsPtr, Tcl_Obj *cmdName, const char *methodName, int withPer_object)
  nonnull(1) nonnull(2) nonnull(3) returns_nonnull;

static const char *
AliasIndex(Tcl_DString *dsPtr, Tcl_Obj *cmdName, const char *methodName, int withPer_object) {

  nonnull_assert(dsPtr != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(methodName != NULL);

  Tcl_DStringInit(dsPtr);
  Tcl_DStringAppend(dsPtr,  ObjStr(cmdName), -1);
  Tcl_DStringAppend(dsPtr,  ",", 1);
  Tcl_DStringAppend(dsPtr,  methodName, -11);
  if (withPer_object == 1) {
    Tcl_DStringAppend(dsPtr,  ",1", 2);
  } else {
    Tcl_DStringAppend(dsPtr,  ",0", 2);
  }
  /*fprintf(stderr, "AI %s\n", Tcl_DStringValue(dsPtr));*/
  return Tcl_DStringValue(dsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AliasAdd --
 *
 *      Add an alias to the alias index
 *
 * Results:
 *      Standard Tcl result
 *
 * Side effects:
 *      Adding value to the hidden associated array.
 *
 *----------------------------------------------------------------------
 */
static int AliasAdd(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName, int withPer_object,
         const char *cmd) nonnull(1) nonnull(2) nonnull(3) nonnull(5);

static int
AliasAdd(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName, int withPer_object,
         const char *cmd) {
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmd != NULL);

  Tcl_SetVar2Ex(interp, NsfGlobalStrings[NSF_ARRAY_ALIAS],
                AliasIndex(dsPtr, cmdName, methodName, withPer_object),
                Tcl_NewStringObj(cmd, -1),
                TCL_GLOBAL_ONLY);

  /*fprintf(stderr, "aliasAdd ::nsf::alias(%s) '%s' returned %p\n",
    AliasIndex(dsPtr, cmdName, methodName, withPer_object), cmd, 1);*/
  Tcl_DStringFree(dsPtr);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasDelete --
 *
 *      Delete an alias from the index
 *
 * Results:
 *      Standard Tcl result
 *
 * Side effects:
 *      delete an entry from the hidden associative array
 *
 *----------------------------------------------------------------------
 */
static int
AliasDelete(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName, int withPer_object) {
  Tcl_DString ds, *dsPtr = &ds;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(methodName != NULL);

  result = Tcl_UnsetVar2(interp, NsfGlobalStrings[NSF_ARRAY_ALIAS],
                         AliasIndex(dsPtr, cmdName, methodName, withPer_object),
                         TCL_GLOBAL_ONLY);

  /*fprintf(stderr, "aliasDelete ::nsf::alias(%s) returned %d (%d)\n",
    AliasIndex(dsPtr, cmdName, methodName, withPer_object), result);*/
  Tcl_DStringFree(dsPtr);

  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasGet --
 *
 *      Get an entry from the alias index
 *
 * Results:
 *      Tcl obj
 *
 * Side effects:
 *      delete an entry from the hidden associative array
 *
 *----------------------------------------------------------------------
 */
static Tcl_Obj *
AliasGet(Tcl_Interp *interp, Tcl_Obj *cmdName, const char *methodName, int withPer_object, int leaveError) {
  Tcl_DString ds, *dsPtr = &ds;
  Tcl_Obj *obj;

  nonnull_assert(interp != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(methodName != NULL);

  obj = Tcl_GetVar2Ex(interp, NsfGlobalStrings[NSF_ARRAY_ALIAS],
                      AliasIndex(dsPtr, cmdName, methodName, withPer_object),
                      TCL_GLOBAL_ONLY);

  /*fprintf(stderr, "aliasGet methodName '%s' returns %p\n", methodName, obj);*/
  Tcl_DStringFree(dsPtr);
  if (obj == NULL && leaveError) {
    NsfPrintError(interp, "could not obtain alias definition for %s %s.",
                  ObjStr(cmdName), methodName);
  }

  return obj;
}

/*
 *----------------------------------------------------------------------
 * AliasDeleteObjectReference --
 *
 *    Delete an alias to an referenced object. Such aliases are
 *    created by registering an alias to an object. This function
 *    distinguishes between a sub-object and an alias to an object,
 *    deletes the alias but never the referenced object.
 *
 * Results:
 *    1 when alias is deleted.
 *
 * Side effects:
 *    Deletes cmd sometimes
 *
 *----------------------------------------------------------------------
 */
static int
AliasDeleteObjectReference(Tcl_Interp *interp, Tcl_Command cmd) {
  NsfObject *referencedObject = NsfGetObjectFromCmdPtr(cmd);

  nonnull_assert(interp != NULL);
  nonnull_assert(cmd != NULL);
  nonnull_assert(referencedObject != NULL);

  /*fprintf(stderr, "AliasDeleteObjectReference on %p obj %p\n", cmd, referencedObject);*/
  if (referencedObject->refCount > 0
      && cmd != referencedObject->id) {
    /*
     * The cmd is an aliased object, reduce the refCount of the
     * object, delete the cmd.
     */
    /*fprintf(stderr, "remove alias %s to %s\n",
      Tcl_GetCommandName(interp, cmd), ObjectName(referencedObject));*/
    NsfCleanupObject(referencedObject, "AliasDeleteObjectReference");
    Tcl_DeleteCommandFromToken(interp, cmd);
    return 1;
  }
  return 0;
}

/*
 *----------------------------------------------------------------------
 * AliasRefetch --
 *
 *    Perform a refetch of an epoched aliased cmd and update the
 *    AliasCmdClientData structure with fresh values.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static int
AliasRefetch(Tcl_Interp *interp, NsfObject *object, const char *methodName, AliasCmdClientData *tcd) {
  Tcl_Obj **listElements, *entryObj, *targetObj;
  int nrElements, withPer_object;
  NsfObject *defObject;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(tcd != NULL);

  defObject = (tcd->class != NULL) ? &(tcd->class->object) : object;

  /*
   * Get the targetObject. Currently, we can get it just via the
   * alias array.
   */
  withPer_object = (tcd->class != NULL) ?  0 : 1;
  entryObj = AliasGet(interp, defObject->cmdName, methodName, withPer_object, 1);
  if (unlikely(entryObj == NULL)) {
    return TCL_ERROR;
  }

  INCR_REF_COUNT(entryObj);
  Tcl_ListObjGetElements(interp, entryObj, &nrElements, &listElements);
  targetObj = listElements[nrElements-1];

  NsfLog(interp, NSF_LOG_NOTICE,
         "trying to refetch an epoched cmd %p as %s -- cmdName %s\n",
         (void *)tcd->aliasedCmd, methodName, ObjStr(targetObj));

  /*
   * Replace cmd and its objProc and clientData with a newly fetched
   * version.
   */
  cmd = Tcl_GetCommandFromObj(interp, targetObj);
  if (cmd != NULL) {
    cmd = GetOriginalCommand(cmd);
    /*fprintf(stderr, "cmd %p epoch %d deleted %.6x\n",
      cmd,
      Tcl_Command_cmdEpoch(cmd),
      Tcl_Command_flags(cmd) & CMD_IS_DELETED);*/
    if (Tcl_Command_flags(cmd) & CMD_IS_DELETED) {
      cmd = NULL;
    }
  }
  if (cmd == NULL) {
    int result = NsfPrintError(interp, "target \"%s\" of alias %s apparently disappeared",
                               ObjStr(targetObj), methodName);
    DECR_REF_COUNT(entryObj);
    return result;
  }

  assert(Tcl_Command_objProc(cmd));

  NsfCommandRelease(tcd->aliasedCmd);
  tcd->objProc    = Tcl_Command_objProc(cmd);
  tcd->aliasedCmd = cmd;
  tcd->clientData = Tcl_Command_objClientData(cmd);
  NsfCommandPreserve(tcd->aliasedCmd);

  DECR_REF_COUNT(entryObj);
  /*
   * Now, we should be able to proceed as planned, we have an
   * non-epoched aliasCmd.
   */
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * AliasDereference --
 *
 *    Dereference a cmd in respect of the the alias structure. If necessary,
 *    this command refetches the aliased command.
 *
 * Results:
 *    NULL, in case refetching fails,
 *    the aliased cmd if it was an alias, or
 *    the original cmd
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
NSF_INLINE static Tcl_Command AliasDereference(Tcl_Interp *interp, NsfObject *object, const char *methodName, Tcl_Command cmd)
  nonnull(1) nonnull(2) nonnull(3) nonnull(4);

NSF_INLINE static Tcl_Command
AliasDereference(Tcl_Interp *interp, NsfObject *object, const char *methodName, Tcl_Command cmd) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  nonnull_assert(cmd != NULL);

  if (unlikely(Tcl_Command_objProc(cmd) == NsfProcAliasMethod)) {
    AliasCmdClientData *tcd = (AliasCmdClientData *)Tcl_Command_objClientData(cmd);

    assert(tcd != NULL);

    if (unlikely(Tcl_Command_cmdEpoch(tcd->aliasedCmd))) {

      /*fprintf(stderr, "NsfProcAliasMethod aliasedCmd %p epoch %p\n",
        tcd->aliasedCmd, Tcl_Command_cmdEpoch(tcd->aliasedCmd));*/

      if (AliasRefetch(interp, object, methodName, tcd) != TCL_OK) {
        return NULL;
      }
    }
    return tcd->aliasedCmd;
  }

  return cmd;
}

#if defined(NSF_ASSEMBLE)
# include "asm/nsfAssemble.c"
#else
static int
NsfAsmMethodCreateCmd(Tcl_Interp *interp, NsfObject *defObject,
                      int with_checkAlways, int withInner_namespace,
                      int withPer_object, NsfObject *regObject,
                      Tcl_Obj *nameObj, Tcl_Obj *argumentsObj, Tcl_Obj *bodyObj) {

  nonnull_assert(interp != NULL);
  nonnull_assert(defObject != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(argumentsObj != NULL);
  nonnull_assert(bodyObj != NULL);

  /*
   * Dummy stub; used, when compiled without NSF_ASSEMBLE
   */
  return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 * SetBooleanFlag --
 *
 *    Set an unsigned int flag based on valueObj
 *
 * Results:
 *    Tcl result code
 *
 * Side effects:
 *    update passed flags
 *
 *----------------------------------------------------------------------
 */

static int SetBooleanFlag(Tcl_Interp *interp, unsigned int *flagsPtr, unsigned int flag, Tcl_Obj *valueObj, int *flagValue)
  nonnull(1) nonnull(2) nonnull(4) nonnull(5);

static int
SetBooleanFlag(Tcl_Interp *interp, unsigned int *flagsPtr, unsigned int flag, Tcl_Obj *valueObj, int *flagValue) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(flagsPtr != NULL);
  nonnull_assert(valueObj != NULL);
  nonnull_assert(flagValue != NULL);

  result = Tcl_GetBooleanFromObj(interp, valueObj, flagValue);
  if (unlikely(result != TCL_OK)) {
    return result;
  }
  if (*flagValue) {
    *flagsPtr |= flag;
  } else {
    *flagsPtr &= ~flag;
  }
  return result;
}

/***********************************************************************
 * Begin generated Next Scripting commands
 ***********************************************************************/

/*
cmd __db_compile_epoch NsfDebugCompileEpoch {}
 */
static int NsfDebugCompileEpoch(Tcl_Interp *interp) nonnull(1);

static int
NsfDebugCompileEpoch(Tcl_Interp *interp) {

  nonnull_assert(interp != NULL);

  Tcl_SetObjResult(interp, Tcl_NewIntObj(((Interp *)interp)->compileEpoch));
  return TCL_OK;
}

/*
cmd __db_show_obj NsfDebugShowObj {
  {-argName "obj"    -required 1 -type tclobj}
}
*/
static int NsfDebugShowObj(Tcl_Interp *interp, Tcl_Obj *objPtr) nonnull(1) nonnull(2);

static int
NsfDebugShowObj(Tcl_Interp *interp, Tcl_Obj *objPtr) {

  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);

  fprintf(stderr, "*** obj %p refCount %d type <%s>\n",
          (void *)objPtr, objPtr->refCount, (objPtr->typePtr != NULL) ? objPtr->typePtr->name : "");

  if (objPtr->typePtr == &NsfObjectMethodObjType
      || objPtr->typePtr == &NsfInstanceMethodObjType
      ) {
    NsfMethodContext *mcPtr = objPtr->internalRep.twoPtrValue.ptr1;
    int currentMethodEpoch = objPtr->typePtr == &NsfObjectMethodObjType ?
      RUNTIME_STATE(interp)->objectMethodEpoch :
      RUNTIME_STATE(interp)->instanceMethodEpoch;
    Tcl_Command cmd = mcPtr->cmd;

    fprintf(stderr, "   method epoch %d max %d cmd %p objProc %p flags %.6x\n",
            mcPtr->methodEpoch, currentMethodEpoch,
            (void *)cmd,
            (cmd != NULL) ? ((void *)((Command *)cmd)->objProc) : NULL,
            mcPtr->flags);
    if (cmd != NULL) {
      fprintf(stderr, "... cmd %p flags %.6x\n", (void *)cmd, Tcl_Command_flags(cmd));
      assert(((Command *)cmd)->objProc != NULL);
    }
    assert( currentMethodEpoch >= mcPtr->methodEpoch);
  }
  return TCL_OK;
}

/*
cmd __db_show_stack NsfShowStackCmd {}
*/
static int
NsfShowStackCmd(Tcl_Interp *interp) {

  nonnull_assert(interp != NULL);

  NsfShowStack(interp);
  return TCL_OK;
}

/*
cmd __db_run_assertions NsfDebugRunAssertionsCmd {}
*/
static int
NsfDebugRunAssertionsCmd(Tcl_Interp *interp) {
  NsfObjectSystem *osPtr;
  NsfCmdList *instances = NULL, *entry;

  nonnull_assert(interp != NULL);

  /*
   * Collect all instances from all object systems.
   */
  for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = osPtr->nextPtr) {
    GetAllInstances(interp, &instances, osPtr->rootClass);
  }

  for (entry = instances; entry; entry = entry->nextPtr) {
#if !defined(NDEBUG)
    NsfObject *object = (NsfObject *)entry->clorobj;
#endif

    assert(object != NULL);
    assert(object->refCount > 0);
    assert(object->cmdName->refCount > 0);
    assert(object->activationCount >= 0);

#if defined(CHECK_ACTIVATION_COUNTS)
    if (object->activationCount > 0) {
      Tcl_CallFrame *framePtr;
      int count = 0;
      NsfClasses *unstackedEntries = RUNTIME_STATE(interp)->cscList;

      /*fprintf(stderr, "DEBUG obj %p %s activationcount %d\n",
        object, ObjectName(object), object->activationCount);*/

      framePtr = (Tcl_CallFrame *)Tcl_Interp_framePtr(interp);
      for (; framePtr; framePtr = Tcl_CallFrame_callerPtr(framePtr)) {
        int frameFlags = Tcl_CallFrame_isProcCallFrame(framePtr);
        NsfCallStackContent *cscPtr =
          ((frameFlags & (FRAME_IS_NSF_METHOD|FRAME_IS_NSF_CMETHOD)) != 0u) ?
          ((NsfCallStackContent *)Tcl_CallFrame_clientData(framePtr)) : NULL;
        if (cscPtr != NULL && cscPtr->self == object) {count ++;}
        if (cscPtr != NULL && (NsfObject *)cscPtr->cl == object) {count ++;}
      }
      for (; unstackedEntries; unstackedEntries = unstackedEntries->nextPtr) {
        NsfCallStackContent *cscPtr = (NsfCallStackContent *)unstackedEntries->cl;
        if (cscPtr != NULL && cscPtr->self == object) {count ++;}
        if (cscPtr != NULL && (NsfObject *)cscPtr->cl == object) {count ++;}
      }

      if (count != object->activationCount) {
        fprintf(stderr, "DEBUG obj %p %s activationcount %d on stack %d; "
                "might be from non-stacked but active call-stack content\n",
                object, ObjectName(object), object->activationCount, count);
        fprintf(stderr, "fixed count %d\n", count);
        /*NsfShowStack(interp);*/
        /*return NsfPrintError(interp, "wrong activation count for object %s", ObjectName(object));*/
      }
    }
#endif

  }
  CmdListFree(&instances, NULL);
  /*fprintf(stderr, "all assertions passed\n");*/

  return TCL_OK;
}

/*
cmd __profile_clear_data NsfProfileClearDataStub {}
*/
static int NsfProfileClearDataStub(Tcl_Interp *interp) nonnull(1);

static int
NsfProfileClearDataStub(Tcl_Interp *interp) {

  nonnull_assert(interp != NULL);

#if defined(NSF_PROFILE)
  NsfProfileClearData(interp);
#endif
  return TCL_OK;
}

/*
cmd __profile_get_data NsfProfileGetDataStub {}
*/
static int NsfProfileGetDataStub(Tcl_Interp *interp) nonnull(1);

static int
NsfProfileGetDataStub(Tcl_Interp *interp) {

  nonnull_assert(interp != NULL);

#if defined(NSF_PROFILE)
  NsfProfileGetData(interp);
#endif
  return TCL_OK;
}

/*
cmd __profile_trace NsfProfileTraceStub {
  {-argName "-enable" -required 1 -nrargs 1 -type boolean}
  {-argName "-verbose" -required 0 -nrargs 1 -type boolean}
  {-argName "-dontsave" -required 0 -nrargs 1 -type boolean}
  {-argName "-builtins" -required 0 -nrargs 1 -type tclobj}
}
*/
static int NsfProfileTraceStub(Tcl_Interp *interp,
                               int withEnable, int withVerbose, int withDontsave,
                               Tcl_Obj *builtins)
  NSF_nonnull(1);

static int
NsfProfileTraceStub(Tcl_Interp *interp, int withEnable, int withVerbose, int withDontsave, Tcl_Obj *builtins) {

  nonnull_assert(interp != NULL);

#if defined(NSF_PROFILE)
  NsfProfileTrace(interp, withEnable, withVerbose, withDontsave, builtins);
#endif
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 * NsfUnsetUnknownArgsCmd --
*
 *    Unset variables set from arguments with the default dummy
 *    default value. The dummy default values are set by
 *    ArgumentDefaults()
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    unsets some variables
 *
 *----------------------------------------------------------------------
 */
/*
cmd __unset_unknown_args NsfUnsetUnknownArgsCmd {}
*/

static int
NsfUnsetUnknownArgsCmd(Tcl_Interp *interp) {
  CallFrame *varFramePtr = Tcl_Interp_varFramePtr(interp);
  Proc *proc = Tcl_CallFrame_procPtr(varFramePtr);

  nonnull_assert(interp != NULL);

  if (likely(proc != NULL)) {
    CompiledLocal *ap;
    Var *varPtr;
    int i;

    for (ap = proc->firstLocalPtr, i = 0; ap; ap = ap->nextPtr, i++) {
      if (!TclIsCompiledLocalArgument(ap)) {
        continue;
      }
      varPtr = &Tcl_CallFrame_compiledLocals(varFramePtr)[i];
      /*fprintf(stderr, "NsfUnsetUnknownArgsCmd var '%s' i %d fi %d var %p flags %.8x obj %p unk %p\n",
              ap->name, i, ap->frameIndex, varPtr, varPtr->flags, varPtr->value.objPtr,
              NsfGlobalObjs[NSF___UNKNOWN__]);*/
      if (varPtr->value.objPtr != NsfGlobalObjs[NSF___UNKNOWN__]) {
        continue;
      }
      /*fprintf(stderr, "NsfUnsetUnknownArgsCmd must unset %s\n", ap->name);*/
      Tcl_UnsetVar2(interp, ap->name, NULL, 0);
    }
  }

  return TCL_OK;
}

/*
cmd asmproc NsfAsmProcCmd {
  {-argName "-ad" -required 0  -nrargs 0 -type switch}
  {-argName "-checkalways" -required 0  -nrargs 0 -type switch}
  {-argName "procName" -required 1 -type tclobj}
  {-argName "arguments" -required 1 -type tclobj}
  {-argName "body" -required 1 -type tclobj}
}
*/
#if !defined(NSF_ASSEMBLE)
static int
NsfAsmProcCmd(Tcl_Interp *interp, int with_ad, int with_checkAlways, Tcl_Obj *nameObj, Tcl_Obj *arguments, Tcl_Obj *body) {

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(arguments != NULL);
  nonnull_assert(body != NULL);

  return TCL_OK;
}
#else
static int
NsfAsmProcCmd(Tcl_Interp *interp, int with_ad, int with_checkAlways, Tcl_Obj *nameObj, Tcl_Obj *arguments, Tcl_Obj *body) {
  NsfParsedParam parsedParam;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(arguments != NULL);
  nonnull_assert(body != NULL);

  /*
   * Parse argument list "arguments" to determine if we should provide
   * nsf parameter handling.
   */
  result = ParamDefsParse(interp, nameObj, arguments,
                          NSF_DISALLOWED_ARG_METHOD_PARAMETER, 0,
                          &parsedParam);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  if (parsedParam.paramDefs != NULL) {
    /*
     * We need parameter handling.
     */
    result = NsfAsmProcAddParam(interp, &parsedParam, nameObj, body, with_ad, with_checkAlways);

  } else {
    /*
     * No parameter handling needed.
     */
    result = NsfAsmProcAddArgs(interp, arguments, nameObj, body, with_ad, with_checkAlways);
  }

  return result;
}
#endif

/*
cmd "cmd::info" NsfCmdInfoCmd {
  {-argName "subcmd" -required 1 -typeName "methodgetcmd" -type "args|body|definition|exists|registrationhandle|definitionhandle|origin|parameter|syntax|type|precondition|postcondition|submethods|returns"}
  {-argName "-context" -required 0 -type object}
  {-argName "methodName" -required 1 -type tclobj}
  {-argName "pattern" -required 0}
} {-nxdoc 1}
*/
static int
NsfCmdInfoCmd(Tcl_Interp *interp, int subcmd, NsfObject *context, Tcl_Obj *methodNameObj, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(methodNameObj != NULL);

  return ListMethodResolve(interp, subcmd, context, pattern, NULL, NULL, methodNameObj, 0);
}

/*
cmd configure NsfConfigureCmd {
  {-argName "configureoption" -required 1 -type "debug|dtrace|filter|profile|softrecreate|objectsystems|keepcmds|checkresults|checkarguments"}
  {-argName "value" -required 0 -type tclobj}
}
*/
static int
NsfConfigureCmd(Tcl_Interp *interp, int configureoption, Tcl_Obj *valueObj) {
  int bool;

  nonnull_assert(interp != NULL);

#if defined(NSF_DTRACE)
  if (NSF_DTRACE_CONFIGURE_PROBE_ENABLED()) {
    /* TODO: opts copied from tclAPI.h; maybe make global value? */
    static const char *opts[] = {
      "debug", "dtrace", "filter", "profile", "trace", "softrecreate",
      "objectsystems", "keepcmds", "checkresults", "checkarguments", NULL};
    NSF_DTRACE_CONFIGURE_PROBE((char *)opts[configureoption-1], (valueObj != NULL) ? ObjStr(valueObj) : NULL);
  }
#endif

  if (configureoption == ConfigureoptionObjectsystemsIdx) {
    NsfObjectSystem *osPtr;
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);

    for (osPtr = RUNTIME_STATE(interp)->objectSystems; osPtr; osPtr = osPtr->nextPtr) {
      Tcl_Obj *osObj = Tcl_NewListObj(0, NULL);
      Tcl_Obj *systemMethods = Tcl_NewListObj(0, NULL);
      int idx;

      Tcl_ListObjAppendElement(interp, osObj, osPtr->rootClass->object.cmdName);
      Tcl_ListObjAppendElement(interp, osObj, osPtr->rootMetaClass->object.cmdName);

      for (idx = 0; Nsf_SystemMethodOpts[idx]; idx++) {
        /*fprintf(stderr, "opt %s %s\n", Nsf_SystemMethodOpts[idx],
          osPtr->methods[idx] ? ObjStr(osPtr->methods[idx]) : "NULL");*/
        if (osPtr->methods[idx] == NULL) {
          continue;
        }
        Tcl_ListObjAppendElement(interp, systemMethods, Tcl_NewStringObj(Nsf_SystemMethodOpts[idx], -1));
        if (osPtr->handles[idx] || osPtr->protected[idx]) {
          Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
          Tcl_ListObjAppendElement(interp, listObj, osPtr->methods[idx]);
          Tcl_ListObjAppendElement(interp, listObj, osPtr->handles[idx]);
          if (osPtr->protected[idx]) {
            Tcl_ListObjAppendElement(interp, listObj, Tcl_NewIntObj(1));
          }
          Tcl_ListObjAppendElement(interp, systemMethods, listObj);
        } else {
          Tcl_ListObjAppendElement(interp, systemMethods, osPtr->methods[idx]);
        }
      }
      Tcl_ListObjAppendElement(interp, osObj, systemMethods);
      Tcl_ListObjAppendElement(interp, list, osObj);
    }
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
  }

  if (configureoption == ConfigureoptionDebugIdx) {
    int level;

    if (valueObj != NULL) {
      int result = Tcl_GetIntFromObj(interp, valueObj, &level);
      if (unlikely(result != TCL_OK)) {
        return result;
      }
      RUNTIME_STATE(interp)->debugLevel = level;
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp),
                  RUNTIME_STATE(interp)->debugLevel);

    return TCL_OK;
  }

  /*
   * All other configure options are boolean.
   */
  if (valueObj != NULL) {
    int result = Tcl_GetBooleanFromObj(interp, valueObj, &bool);
    if (unlikely(result != TCL_OK)) {
      return result;
    }
  }

  switch (configureoption) {
  case ConfigureoptionFilterIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doFilters));
    if (valueObj != NULL) {
      RUNTIME_STATE(interp)->doFilters = bool;
    }
    break;

  case ConfigureoptionProfileIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doProfile));
    if (valueObj != NULL) {
#if defined(NSF_PROFILE)
      RUNTIME_STATE(interp)->doProfile = bool;
#else
      NsfLog(interp, NSF_LOG_WARN, "No profile support compiled in");
#endif
    }
    break;

  case ConfigureoptionTraceIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doTrace));
    if (valueObj != NULL) {
#if defined(NSF_PROFILE)
      RUNTIME_STATE(interp)->doTrace = bool;
      /*
       * Turn automically profiling on&off, when trace is turned on/off
       */
      RUNTIME_STATE(interp)->doProfile = bool;
#else
      NsfLog(interp, NSF_LOG_WARN, "No profile support compiled in");
#endif
    }
    break;

  case ConfigureoptionSoftrecreateIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doSoftrecreate));
    if (valueObj != NULL) {
      RUNTIME_STATE(interp)->doSoftrecreate = bool;
    }
    break;

  case ConfigureoptionKeepcmdsIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doKeepcmds));
    if (valueObj != NULL) {
      RUNTIME_STATE(interp)->doKeepcmds = bool;
    }
    break;

  case ConfigureoptionCheckresultsIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doCheckResults));
    if (valueObj != NULL) {
      RUNTIME_STATE(interp)->doCheckResults = bool;
    }
    break;

  case ConfigureoptionCheckargumentsIdx:
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (RUNTIME_STATE(interp)->doCheckArguments) != 0);
    if (valueObj != NULL) {
      RUNTIME_STATE(interp)->doCheckArguments = (bool != 0) ? NSF_ARGPARSE_CHECK : 0;
    }
    break;
  }
  return TCL_OK;
}


/*
cmd colon NsfColonCmd {
  {-argName "args" -type allargs}
}
*/
static int
NsfColonCmd(Tcl_Interp *interp, int nobjc, Tcl_Obj *CONST nobjv[]) {
  const char *methodName = ObjStr(nobjv[0]);
  NsfObject *self = GetSelfObj(interp);

  nonnull_assert(interp != NULL);

  if (unlikely(self == NULL)) {
    return NsfNoCurrentObjectError(interp, methodName);
  }
  /*fprintf(stderr, "Colon dispatch %s.%s (%d)\n",
    ObjectName(self), ObjStr(nobjv[0]), nobjc);*/

  if (likely(!(*methodName == ':' && *(methodName + 1) == '\0'))) {
    return ObjectDispatch(self, interp, nobjc, nobjv, NSF_CM_NO_SHIFT);
  }
  /*
   * First arg is a single colon and no other args are given.
   */
  if (nobjc <= 1) {
    Tcl_SetObjResult(interp, self->cmdName);
    return TCL_OK;
  }

  /*
   * Multiple arguments were given.
   */
  methodName = ObjStr(nobjv[1]);

  if (*methodName != '-') {
    /*
     * No need to parse arguments (local, intrinsic, ...).
     */
    return ObjectDispatch(self, interp, nobjc, nobjv, 0);
  } else {
    ParseContext pc;
    int withIntrinsic, withLocal, withSystem, flags;
    Tcl_Obj *methodObj;

    /*
     * Parse arguments, use definitions from nsf::my
     */
    if (unlikely(ArgumentParse(interp, nobjc, nobjv, NULL, nobjv[0],
                               method_definitions[NsfMyCmdIdx].paramDefs,
                               method_definitions[NsfMyCmdIdx].nrParameters,
                               0, NSF_ARGPARSE_BUILTIN, &pc) != TCL_OK)) {
      return TCL_ERROR;
    }

    withIntrinsic = (int)PTR2INT(pc.clientData[0]);
    withLocal     = (int)PTR2INT(pc.clientData[1]);
    withSystem    = (int)PTR2INT(pc.clientData[2]);
    methodObj     = (Tcl_Obj *)pc.clientData[3];

    assert(pc.status == 0);

    if ((withIntrinsic && withLocal)
        || (withIntrinsic && withSystem)
        || (withLocal && withSystem)) {
      return NsfPrintError(interp, "flags '-intrinsic', '-local' and '-system' are mutual exclusive");
    }

    flags = NSF_CSC_IMMEDIATE;
    if (withIntrinsic != 0) {flags |= NSF_CM_INTRINSIC_METHOD;}
    if (withLocal != 0)     {flags |= NSF_CM_LOCAL_METHOD;}
    if (withSystem != 0)    {flags |= NSF_CM_SYSTEM_METHOD;}
    return CallMethod(self, interp, methodObj, (nobjc-pc.lastObjc)+2, nobjv+pc.lastObjc, flags);
  }
}

/*
cmd "directdispatch" NsfDirectDispatchCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-frame" -required 0 -nrargs 1 -type "method|object|default" -default "default"}
  {-argName "command" -required 1 -type tclobj}
  {-argName "args"  -type args}
}
*/
static int
NsfDirectDispatchCmd(Tcl_Interp *interp, NsfObject *object, int withFrame,
                     Tcl_Obj *commandObj, int nobjc, Tcl_Obj *CONST nobjv[]) {
  int result;
  const char *methodName = ObjStr(commandObj);
  Tcl_Command cmd, importedCmd;
  CallFrame frame, *framePtr = &frame;
  Tcl_ObjCmdProc *proc;
  unsigned int flags = 0;
  int useCmdDispatch = 1;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(commandObj != NULL);

  /*fprintf(stderr, "NsfDirectDispatchCmd obj=%s, cmd m='%s' oc %d\n", ObjectName(object), methodName, nobjc);*/

  if (unlikely(*methodName != ':')) {
    return NsfPrintError(interp, "method name '%s' must be fully qualified", methodName);
  }

  /*
   * We have a fully qualified name of a Tcl command that will be dispatched.
   */

  cmd = Tcl_GetCommandFromObj(interp, commandObj);
  if (likely(cmd != NULL)) {
    importedCmd = TclGetOriginalCommand(cmd);
    if (unlikely(importedCmd != NULL)) {
      cmd = importedCmd;
    }
  }

  if (unlikely(cmd == NULL)) {
    return NsfPrintError(interp, "cannot lookup command '%s'", methodName);
  }

  proc = Tcl_Command_objProc(cmd);
  if (proc == TclObjInterpProc ||
      proc == NsfForwardMethod ||
      proc == NsfObjscopedMethod ||
      proc == NsfSetterMethod ||
      CmdIsNsfObject(cmd)) {

    if (withFrame && withFrame != FrameDefaultIdx) {
      return NsfPrintError(interp, "cannot use -frame object|method in dispatch for command '%s'",
                           methodName);
    }
    useCmdDispatch = 0;
  } else {
    if (unlikely(withFrame == FrameMethodIdx)) {
      useCmdDispatch = 0;
    } else {
      useCmdDispatch = 1;
    }
  }

  /*
   * If "withFrame == FrameObjectIdx" is specified, a call-stack frame is
   * pushed to make instance variables accessible for the command.
   */
  if (unlikely(withFrame == FrameObjectIdx)) {
    Nsf_PushFrameObj(interp, object, framePtr);
    flags = NSF_CSC_IMMEDIATE;
  }
  /*
   * Since we know, that we are always called with a full argument
   * vector, we can include the cmd name in the objv by using
   * nobjv-1; this way, we avoid a memcpy().
   */
  if (useCmdDispatch != 0) {

    if (NSF_DTRACE_METHOD_ENTRY_ENABLED()) {
      NSF_DTRACE_METHOD_ENTRY(ObjectName(object),
                              "",
                              (char *)methodName,
                              nobjc, (Tcl_Obj **)nobjv);
    }

    result = CmdMethodDispatch(object, interp, nobjc+1, nobjv-1,
                               object, cmd, NULL);
  } else {
    /*
     * If "withFrame == FrameMethodIdx" is specified, a call-stack frame is
     * pushed to make instance variables accessible for the command.
     */
    if (unlikely(withFrame == FrameMethodIdx)) {
      flags = NSF_CSC_FORCE_FRAME|NSF_CSC_IMMEDIATE;
    }

    result = MethodDispatch(object, interp,
                            nobjc+1, nobjv-1, cmd, object,
                            NULL /*NsfClass *cl*/,
                            Tcl_GetCommandName(interp, cmd),
                            NSF_CSC_TYPE_PLAIN, flags);
  }

  if (unlikely(withFrame == FrameObjectIdx)) {
    Nsf_PopFrameObj(interp, framePtr);
  }

  return result;
}


/*
cmd "dispatch" NsfDispatchCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-intrinsic" -required 0 -nrargs 0}
  {-argName "-system" -required 0 -nrargs 0}
  {-argName "command" -required 1 -type tclobj}
  {-argName "args"  -type args}
}
*/
static int
NsfDispatchCmd(Tcl_Interp *interp, NsfObject *object,
               int withIntrinsic, int withSystem,
               Tcl_Obj *commandObj, int nobjc, Tcl_Obj *CONST nobjv[]) {
  unsigned int flags = NSF_CM_NO_UNKNOWN|NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS|NSF_CM_NO_SHIFT;
  Tcl_Obj *CONST*objv = nobjv-1;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(commandObj != NULL);

  /*
   * We use the construct tclObj + args in the spec to enforce that at least a
   * commandName is specified (this way we allow empty "args", and can provide
   * a nice error message, if cmdName is not specified). Since the we know
   * that the commandObj has to be right before "args" in the objv, we can
   * decrement the nobjv to obtain objv (and increment nobjc), be we make sure
   * that this assumption is correct.
   */
  assert(objv[0] == commandObj);
  assert(ISOBJ(commandObj));

  nobjc++;

#if 0
  {int i;
  fprintf(stderr, "NsfDispatchCmd %s method %s oc %2d", ObjectName(object), ObjStr(commandObj), nobjc);
  for(i = 0; i < nobjc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
  fprintf(stderr, "\n");
  }
#endif

  if (unlikely(withIntrinsic && withSystem)) {
    return NsfPrintError(interp, "flags '-intrinsic' and '-system' are mutual exclusive");
  }

  /*
   * Dispatch the command the method from the precedence order, with filters
   * etc. -- strictly speaking unnecessary, but this function can be used to
   * call protected methods and provide the flags '-intrinsics' and '-system'.
   */

  if (withIntrinsic != 0) {
    flags |= NSF_CM_INTRINSIC_METHOD;
  }
  if (withSystem != 0) {
    flags |= NSF_CM_SYSTEM_METHOD;
  }

  /*
   * Since we know, that we are always called with a full argument
   * vector, we can include the cmd name in the objv by using
   * nobjv-1; this way, we avoid a memcpy().
   */
  return ObjectDispatch(object, interp,  nobjc, objv, flags);
}

/*
cmd finalize NsfFinalizeCmd {
  {-argName "-keepvars" -required 0 -nrargs 0}
}
*/
static int
NsfFinalizeCmd(Tcl_Interp *interp, int withKeepvars) {
  int result;

  nonnull_assert(interp != NULL);

#if defined(NSF_PROFILE)
  /*
   * Check, if profile trace is still running. If so, delete it here.
   * Interestingly, NsfLog() seems to be unavaliable at this place.
   */
  if (RUNTIME_STATE(interp)->doTrace == 1) {
    NsfLog(interp, NSF_LOG_WARN, "tracing is still running, deactivate due to cleanup");
    NsfProfileTrace(interp, 0, 0, 0, NULL);
  }
#endif

#if defined(NSF_STACKCHECK)
  {NsfRuntimeState *rst = RUNTIME_STATE(interp);

    NsfLog(interp, NSF_LOG_WARN, "Stack max usage %ld",
           labs(rst->maxStack - rst->bottomOfStack));
  }
#endif

  /*fprintf(stderr, "+++ call tcl-defined exit handler\n");*/

  /*
   * Evaluate user-defined exit handler.
   */
  result = Tcl_Eval(interp, "::nsf::__exithandler");

  if (unlikely(result != TCL_OK)) {
    fprintf(stderr, "User defined exit handler contains errors!\n"
            "Error in line %d: %s\nExecution interrupted.\n",
            Tcl_GetErrorLine(interp), ObjStr(Tcl_GetObjResult(interp)));
  }

  ObjectSystemsCleanup(interp, withKeepvars);


#ifdef DO_CLEANUP
  {
    NsfRuntimeState *rst = RUNTIME_STATE(interp);
# if defined(CHECK_ACTIVATION_COUNTS)
    assert(rst->cscList == NULL);
# endif
    /*fprintf(stderr, "CLEANUP TOP NS\n");*/
    Tcl_Export(interp, rst->NsfNS, "", 1);
    if (rst->NsfClassesNS != NULL) {
      MEM_COUNT_FREE("TclNamespace",rst->NsfClassesNS);
      Tcl_DeleteNamespace(rst->NsfClassesNS);
    }
    if (rst->NsfNS != NULL) {
      MEM_COUNT_FREE("TclNamespace",rst->NsfNS);
      Tcl_DeleteNamespace(rst->NsfNS);
    }
  }
#endif
  return TCL_OK;
}


/*
cmd interp NsfInterpObjCmd {
  {-argName "name"}
  {-argName "args" -type allargs}
}
*/
/* create a slave interp that calls Next Scripting Init */
static int
NsfInterpObjCmd(Tcl_Interp *interp, const char *name, int objc, Tcl_Obj *CONST objv[]) {

  nonnull_assert(interp != NULL);
  nonnull_assert(name != NULL);

  /* create a fresh Tcl interpreter, or pass command to an existing one */
  if (unlikely(NsfCallCommand(interp, NSF_INTERP, objc, objv) != TCL_OK)) {
    return TCL_ERROR;
  }

  /*
   * Upon [interp create], set up NSF for the new child interp by running
   * Nsf_Init()
   */

  if (isCreateString(name)) {
    Tcl_Obj *slaveCmdObj;
    Tcl_Interp *slavePtr;

    /*
     * Tcl_InterpObjCmd() stores the newly created child interp's command name
     * in the interp result store.
     */

    slaveCmdObj = Tcl_GetObjResult(interp);
    slavePtr = Tcl_GetSlave(interp, ObjStr(slaveCmdObj));

    if (slavePtr == NULL) {
      return NsfPrintError(interp, "creation of slave interpreter failed");
    }
    if (unlikely(Nsf_Init(slavePtr) == TCL_ERROR)) {
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/*
cmd is NsfIsCmd {
  {-argName "-complain"  -nrargs 0}
  {-argName "-configure" -nrargs 0}
  {-argName "-name" -required 0}
  {-argName "constraint" -required 1 -type tclobj}
  {-argName "value" -required 1 -type tclobj}
} {-nxdoc 1}
*/
static int
NsfIsCmd(Tcl_Interp *interp,
         int withComplain,
         int doConfigureParameter,
         const char *name,
         Tcl_Obj *constraintObj,
         Tcl_Obj *valueObj) {
  Nsf_Param *paramPtr = NULL;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(constraintObj != NULL);
  nonnull_assert(valueObj != NULL);

  result = ParameterCheck(interp, constraintObj, valueObj,
                          (name != NULL) ? name : "value:", 1,
                          (name != NULL),
                          doConfigureParameter,
                          &paramPtr);

  if (unlikely(paramPtr == NULL)) {
    /*
     * We could not convert the arguments. Even with noComplain, we
     * report the invalid converter spec as exception.
     */
    return TCL_ERROR;
  }

  if (paramPtr->converter == ConvertViaCmd
      && (withComplain == 0 || result == TCL_OK)) {
    Tcl_ResetResult(interp);
  }

  if (withComplain == 0) {
    Tcl_SetIntObj(Tcl_GetObjResult(interp), (result == TCL_OK));
    result = TCL_OK;
  } else if (likely(result == TCL_OK)) {
    Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
  }

  return result;
}

/*
cmd method::alias NsfMethodAliasCmd {
  {-argName "object" -type object}
  {-argName "-per-object"}
  {-argName "methodName"}
  {-argName "-frame" -required 0 -nrargs 1 -type "method|object|default" -default "default"}
  {-argName "-protection" -required 0 -type "call-protected|redefine-protected|none" -default "none"}
  {-argName "cmdName" -required 1 -type tclobj}
}
*/
static int
NsfMethodAliasCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object,
                  const char *methodName, int withFrame, int withProtection,
                  Tcl_Obj *cmdName) {
  Tcl_ObjCmdProc *objProc, *newObjProc = NULL;
  Tcl_CmdDeleteProc *deleteProc = NULL;
  AliasCmdClientData *tcd = NULL; /* make compiler happy */
  Tcl_Command cmd, oldCmd, newCmd = NULL;
  Tcl_Namespace *nsPtr;
  int result;
  unsigned int flags;
  NsfClass *cl = (withPer_object || ! NsfObjectIsClass(object)) ? NULL : (NsfClass *)object;
  NsfObject *oldTargetObject, *newTargetObject;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodName != NULL);
  assert(*methodName != ':');
  nonnull_assert(cmdName != NULL);

  cmd = Tcl_GetCommandFromObj(interp, cmdName);
  if (cmd == NULL) {
    return NsfPrintError(interp, "cannot lookup command '%s'", ObjStr(cmdName));
  }

  cmd = GetOriginalCommand(cmd);
  objProc = Tcl_Command_objProc(cmd);
  assert(objProc != NULL);

  /* objProc is either ...
   *
   * 1. NsfObjDispatch: a command representing an Next Scripting object
   *
   * 2. TclObjInterpProc: a cmd standing for a Tcl proc (including
   *    Next Scripting methods), verified through CmdIsProc() -> to be
   *    wrapped by NsfProcAliasMethod()
   *
   * 3. NsfForwardMethod: an Next Scripting forwarder
   *
   * 4. NsfSetterMethod: an Next Scripting setter
   *
   * 5. arbitrary Tcl commands (e.g. set, ..., ::nsf::relation, ...)
   *
   */

  if (withFrame == FrameObjectIdx) {
    newObjProc = NsfObjscopedMethod;
  }

  /*
   * We need to perform a defensive lookup of a previously defined
   * object-alias under the given methodName.
   */
  nsPtr = (cl != NULL) ? cl->nsPtr : object->nsPtr;
  oldCmd = (nsPtr != NULL) ? FindMethod(nsPtr, methodName) : NULL;
  newTargetObject = NsfGetObjectFromCmdPtr(cmd);

  if (oldCmd != NULL) {
    oldTargetObject = NsfGetObjectFromCmdPtr(oldCmd);
    /* fprintf(stderr, "oldTargetObject %p flags %.6x newTargetObject %p\n",
       oldTargetObject, (oldTargetObject != NULL) ? oldTargetObject->flags : 0, newTargetObject);*/

    /*
     * We might have to decrement the reference counter on an previously
     * aliased object. Decrement the reference count to the old aliased object
     * only, when it is different to the new target Object.
     */

    if (oldTargetObject != NULL && oldTargetObject != newTargetObject) {
      /*fprintf(stderr, "--- releasing old target object %p refCount %d\n",
        oldTargetObject, oldTargetObject->refCount);*/
      assert(oldTargetObject->refCount > 0);
      AliasDeleteObjectReference(interp, oldCmd);
    }
  } else {
    oldTargetObject = NULL;
  }

  if (newTargetObject != NULL) {
    /*
     * We set now for every alias to an object a stub proc, such we can
     * distinguish between cases, where the user wants to create a method, and
     * between cases, where object-invocation via method interface might
     * happen.
     */
    newObjProc = NsfProcAliasMethod;

  } else if (CmdIsProc(cmd)) {
    /*
     * When we have a Tcl proc|nsf-method as alias, then use the
     * wrapper, which will be deleted automatically when the original
     * proc/method is deleted.
     */
    newObjProc = NsfProcAliasMethod;

    if (objProc == TclObjInterpProc) {
      /*
       * We have an alias to a tcl proc;
       */
      Proc *procPtr = (Proc *)Tcl_Command_objClientData(cmd);
      Tcl_Obj *bodyObj = (procPtr != NULL) ? procPtr->bodyPtr : NULL;

      if (bodyObj && bodyObj->typePtr == Nsf_OT_byteCodeType) {
        /*
         * Flush old byte code
         */
        /*fprintf(stderr, "flush byte code\n");*/
        bodyObj->typePtr->freeIntRepProc(bodyObj);
      }
    }

    if (withFrame && withFrame != FrameDefaultIdx) {
      return NsfPrintError(interp,
                          "cannot use -frame object|method in alias for scripted command '%s'",
                           ObjStr(cmdName));
    }
  }

  if (newObjProc != NULL) {
    /* add a wrapper */
    /*fprintf(stderr, "NsfMethodAliasCmd add wrapper cmd %p\n", cmd);*/
    NsfCommandPreserve(cmd);
    tcd = NEW(AliasCmdClientData);
    tcd->cmdName    = object->cmdName;
    tcd->interp     = interp; /* just for deleting the alias */
    tcd->object     = NULL;
    tcd->class            = (cl != NULL) ? (NsfClass *) object : NULL;
    tcd->objProc    = objProc;
    tcd->aliasedCmd = cmd;
    tcd->clientData = Tcl_Command_objClientData(cmd);

    objProc         = newObjProc;
    deleteProc      = AliasCmdDeleteProc;
    if (tcd->cmdName != NULL) {INCR_REF_COUNT(tcd->cmdName);}
  } else {
    /*
     * Call the command directly (must be a c-implemented command not
     * depending on a volatile client data)
     */
    tcd = Tcl_Command_objClientData(cmd);
    /*fprintf(stderr, "NsfMethodAliasCmd no wrapper cmd %p\n", cmd);*/
  }

  switch (withProtection) {
  case ProtectionCall_protectedIdx:     flags = NSF_CMD_CALL_PROTECTED_METHOD; break;
  case ProtectionRedefine_protectedIdx: flags = NSF_CMD_REDEFINE_PROTECTED_METHOD; break;
  default: flags = 0u;
  }

  if (cl != NULL) {
    result = NsfAddClassMethod(interp, (Nsf_Class *)cl, methodName,
                               objProc, tcd, deleteProc, flags);
    nsPtr = cl->nsPtr;
  } else {
    result = NsfAddObjectMethod(interp, (Nsf_Object *)object, methodName,
                                objProc, tcd, deleteProc, flags);
    nsPtr = object->nsPtr;
  }

  if (likely(result == TCL_OK)) {
    newCmd = FindMethod(nsPtr, methodName);
  }

#if defined(WITH_IMPORT_REFS)
  if (newObjProc != NULL) {
    /*
     * Define the reference chain like for 'namespace import' to
     * obtain automatic deletes when the original command is deleted.
     */
    ImportRef *refPtr = (ImportRef *) ckalloc(sizeof(ImportRef));
    refPtr->importedCmdPtr = (Command *) newCmd;
    refPtr->nextPtr = ((Command *) tcd->aliasedCmd)->importRefPtr;
    ((Command *) tcd->aliasedCmd)->importRefPtr = refPtr;
    tcd->aliasCmd = newCmd;
  }
#else
  if (newObjProc != NULL) {
    tcd->aliasCmd = newCmd;
  }
#endif

  if (newCmd != NULL) {
    AliasAdd(interp, object->cmdName, methodName, cl == NULL, ObjStr(cmdName));

    if (withFrame == FrameMethodIdx) {
      Tcl_Command_flags(newCmd) |= NSF_CMD_NONLEAF_METHOD;
      /*fprintf(stderr, "setting aliased for cmd %p %s flags %.6x, tcd = %p\n",
        newCmd, methodName, Tcl_Command_flags(newCmd), tcd);*/
    }

    Tcl_SetObjResult(interp, MethodHandleObj(object, cl == NULL, methodName));
    result = TCL_OK;
  }

  return result;
}

/*
cmd method::assertion NsfMethodAssertionCmd {
  {-argName "object" -type object}
  {-argName "assertionsubcmd" -required 1 -type "check|object-invar|class-invar"}
  {-argName "arg" -required 0 -type tclobj}
}

  Make "::nsf::assertion" a cmd rather than a method, otherwise we
  cannot define e.g. a "method check options {...}" to reset the check
  options in case of a failed option, since assertion checking would
  be applied on the sketched method already.
*/

static int
NsfMethodAssertionCmd(Tcl_Interp *interp, NsfObject *object, int subcmd, Tcl_Obj *arg) {
#if defined(NSF_WITH_ASSERTIONS)
  NsfClass *class;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  switch (subcmd) {
  case AssertionsubcmdCheckIdx:
    if (arg != NULL) {
      return AssertionSetCheckOptions(interp, object, arg);
    } else {
      return AssertionListCheckOption(interp, object);
    }
    break;

  case AssertionsubcmdObject_invarIdx:
    if (arg != NULL) {
      NsfObjectOpt *opt = NsfRequireObjectOpt(object);
      AssertionSetInvariants(interp, &opt->assertions, arg);
    } else {
      if (object->opt != NULL && object->opt->assertions != NULL) {
        Tcl_SetObjResult(interp, AssertionList(interp, object->opt->assertions->invariants));
      }
    }
    break;

  case AssertionsubcmdClass_invarIdx:

    if (!NsfObjectIsClass(object)) {
      return NsfPrintError(interp, "object is not a class");
    }

    class = (NsfClass *)object;
    if (arg != NULL) {
      NsfClassOpt *opt = NsfRequireClassOpt(class);
      AssertionSetInvariants(interp, &opt->assertions, arg);
    } else {
      if (class->opt != NULL && class->opt->assertions != NULL) {
        Tcl_SetObjResult(interp, AssertionList(interp, class->opt->assertions->invariants));
      }
    }
  }
#endif
  return TCL_OK;
}

/*
cmd method::create NsfMethodCreateCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-checkalways" -required 0 -nrargs 0 -type switch}
  {-argName "-inner-namespace"}
  {-argName "-per-object"}
  {-argName "-reg-object" -required 0 -nrargs 1 -type object}
  {-argName "name" -required 1 -type tclobj}
  {-argName "arguments" -required 1 -type tclobj}
  {-argName "body" -required 1 -type tclobj}
  {-argName "-precondition"  -nrargs 1 -type tclobj}
  {-argName "-postcondition" -nrargs 1 -type tclobj}
}
*/
static int
NsfMethodCreateCmd(Tcl_Interp *interp, NsfObject *defObject,
                   int withCheckAlways, int withInner_namespace,
                   int withPer_object, NsfObject *regObject,
                   Tcl_Obj *nameObj, Tcl_Obj *arguments, Tcl_Obj *body,
                   Tcl_Obj *withPrecondition, Tcl_Obj *withPostcondition) {
  NsfClass *cl =
    (withPer_object || ! NsfObjectIsClass(defObject)) ?
    NULL : (NsfClass *)defObject;

  nonnull_assert(interp != NULL);
  nonnull_assert(defObject != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(arguments != NULL);
  nonnull_assert(body != NULL);

  if (cl == NULL) {
    RequireObjNamespace(interp, defObject);
  }
  return MakeMethod(interp, defObject, regObject, cl,
                    nameObj, arguments, body,
                    withPrecondition, withPostcondition,
                    withInner_namespace, (withCheckAlways != 0) ? NSF_ARGPARSE_CHECK : 0);
}

/*
cmd "method::delete" NsfMethodDeleteCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "methodName" -required 1 -type tclobj}
}
*/
static int
NsfMethodDeleteCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object,
                   Tcl_Obj *methodNameObj) {
  NsfObject *regObject, *defObject;
  const char *methodName1 = NULL;
  NsfClass *cl = withPer_object == 0 && NsfObjectIsClass(object) ? (NsfClass *)object : NULL;
  int fromClassNS = cl != NULL, result;
  Tcl_DString ds, *dsPtr = &ds;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodNameObj != NULL);

  Tcl_DStringInit(dsPtr);

  cmd = ResolveMethodName(interp, (cl != NULL) ? cl->nsPtr : object->nsPtr, methodNameObj,
                          dsPtr, &regObject, &defObject, &methodName1, &fromClassNS);

  /*fprintf(stderr,
          "NsfMethodDeleteCmd method %s '%s' object %p regObject %p defObject %p cl %p fromClass %d cmd %p\n",
          ObjStr(methodNameObj), methodName1, object, regObject, defObject, cl, fromClassNS, cmd);*/

  if (cmd != NULL) {
    methodName1 = Tcl_GetCommandName(interp, cmd);
    if (defObject != NULL) {
      cl = withPer_object == 0 && NsfObjectIsClass(defObject) ? (NsfClass *)defObject : NULL;
    } else {
      defObject = object;
    }

    if (RUNTIME_STATE(interp)->exitHandlerDestroyRound == NSF_EXITHANDLER_OFF) {
      result = (cl != NULL) ?
        NsfRemoveClassMethod(interp,  (Nsf_Class *)defObject, methodName1) :
        NsfRemoveObjectMethod(interp, (Nsf_Object *)defObject, methodName1);
    } else {
      result = TCL_OK;
    }

  } else {
    result = NsfPrintError(interp, "%s: %s method '%s' does not exist",
                           ObjectName(object), (withPer_object == 1) ? "object specific" : "instance",
                           ObjStr(methodNameObj));
  }

  Tcl_DStringFree(dsPtr);

  return result;
}

/*
cmd method::forward NsfMethodForwardCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object" -required 0 -nrargs 0 -type switch}
  {-argName "method" -required 1 -type tclobj}
  {-argName "-default" -type tclobj}
  {-argName "-earlybinding" -nrargs 0}
  {-argName "-prefix" -type tclobj}
  {-argName "-frame" -nrargs 1 -type "object|method|default" -default default}
  {-argName "-verbose" -nrargs 0}
  {-argName "target" -type tclobj}
  {-argName "args" -type args}
}
*/
static int
NsfMethodForwardCmd(Tcl_Interp *interp,
                    NsfObject *object, int withPer_object,
                    Tcl_Obj *methodObj,
                    Tcl_Obj *withDefault,
                    int withEarlybinding,
                    Tcl_Obj *withOnerror,
                    Tcl_Obj *withMethodprefix,
                    int withFrame, int withVerbose,
                    Tcl_Obj *target, int nobjc, Tcl_Obj *CONST nobjv[]) {
  ForwardCmdClientData *tcd = NULL;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObj != NULL);

  result = ForwardProcessOptions(interp, methodObj,
                                 withDefault, withEarlybinding,
                                 withOnerror, withMethodprefix,
                                 withFrame, withVerbose,
                                 target, nobjc, nobjv, &tcd);

  if (likely(result == TCL_OK)) {
    const char *methodName = NSTail(ObjStr(methodObj));
    NsfClass *cl =
      (withPer_object || ! NsfObjectIsClass(object)) ?
      NULL : (NsfClass *)object;

    tcd->object = object;

    if (cl == NULL) {
      result = NsfAddObjectMethod(interp, (Nsf_Object *)object, methodName,
                                  (Tcl_ObjCmdProc *)NsfForwardMethod,
                                  tcd, ForwardCmdDeleteProc, 0);
    } else {
      result = NsfAddClassMethod(interp, (Nsf_Class *)cl, methodName,
                                 (Tcl_ObjCmdProc *)NsfForwardMethod,
                                 tcd, ForwardCmdDeleteProc, 0);
    }
    if (likely(result == TCL_OK)) {
      Tcl_SetObjResult(interp, MethodHandleObj(object, withPer_object, methodName));
    }
  }

  if (result != TCL_OK && tcd) {
    ForwardCmdDeleteProc(tcd);
  }
  return result;
}


/*
cmd "method::forward::property" NsfForwardPropertyCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object" -required 0 -nrargs 0 -type switch}
  {-argName "methodName" -required 1 -type tclobj}
  {-argName "forwardProperty" -required 1 -type "target|verbose"}
  {-argName "value" -type tclobj}
}
*/
static int
NsfForwardPropertyCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object,
                     Tcl_Obj *methodObj, int methodproperty, Tcl_Obj *valueObj) {
  ForwardCmdClientData *tcd;
  Tcl_ObjCmdProc       *procPtr;
  Tcl_Command           cmd;
  NsfObject            *defObject;
  NsfClass             *cl;
  int                   fromClassNS;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObj != NULL);

  cl = withPer_object == 0 && NsfObjectIsClass(object) ? (NsfClass *)object : NULL;
  fromClassNS = cl != NULL;

  cmd = ResolveMethodName(interp, (cl != NULL) ? cl->nsPtr : object->nsPtr, methodObj,
                          NULL, NULL, &defObject, NULL, &fromClassNS);

  if (unlikely(cmd == NULL)) {
    return NsfPrintError(interp, "cannot lookup %s method '%s' for %s",
                         cl == NULL ? "object " : "",
                         ObjStr(methodObj), ObjectName(object));
  }

  procPtr = Tcl_Command_objProc(cmd);
  if (procPtr != NsfForwardMethod) {
    return NsfPrintError(interp, "%s is not a forwarder method",
                         ObjStr(methodObj));
  }

  tcd = (ForwardCmdClientData *)Tcl_Command_objClientData(cmd);
  if (tcd == NULL) {
    return NsfPrintError(interp, "forwarder method has no client data");
  }

  switch (methodproperty) {
  case ForwardpropertyTargetIdx:
    if (valueObj != NULL) {
      DECR_REF_COUNT(tcd->cmdName);
      INCR_REF_COUNT(valueObj);
      tcd->cmdName = valueObj;
    }
    // should we return old or new value? /class/set/... return new value, /configure/ often the old.
    Tcl_SetObjResult(interp, tcd->cmdName);
    break;

  case ForwardpropertyPrefixIdx:
    if (valueObj != NULL) {
      DECR_REF_COUNT(tcd->prefix);
      INCR_REF_COUNT(valueObj);
      tcd->prefix = valueObj;
    }
    Tcl_SetObjResult(interp, tcd->prefix);
    break;

  case ForwardpropertyVerboseIdx:
    if (valueObj != NULL) {
      Tcl_GetBooleanFromObj(interp, valueObj, &tcd->verbose);
    }
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(tcd->verbose));
    break;
  }

  return TCL_OK;
}

/*
cmd ::method::property NsfMethodPropertyCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "methodName" -required 1 -type tclobj}
  {-argName "methodproperty" -required 1 -type "class-only|call-private|call-protected|redefine-protected|returns|slotobj"}
  {-argName "value" -type tclobj}
}
*/
static int
NsfMethodPropertyCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object,
                     Tcl_Obj *methodObj, int methodproperty, Tcl_Obj *valueObj) {
  const char *methodName = ObjStr(methodObj);
  NsfObject *defObject;
  Tcl_Command cmd;
  NsfClass *cl = withPer_object == 0 && NsfObjectIsClass(object) ? (NsfClass *)object : NULL;
  int fromClassNS = cl != NULL;
  unsigned int flag;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObj != NULL);

  cmd = ResolveMethodName(interp, (cl != NULL) ? cl->nsPtr : object->nsPtr, methodObj,
                          NULL, NULL, &defObject, NULL, &fromClassNS);
  /*fprintf(stderr, "methodProperty for method '%s' prop %d value %s => cl %p cmd %p\n",
    methodName, methodproperty, (valueObj != NULL) ? ObjStr(valueObj) : "NULL", cl, cmd);*/


  if (unlikely(cmd == NULL)) {
    return NsfPrintError(interp, "cannot lookup %s method '%s' for %s",
                         cl == NULL ? "object " : "",
                         methodName, ObjectName(object));
  }

  switch (methodproperty) {
  case MethodpropertyClass_onlyIdx: /* fall through */
  case MethodpropertyCall_privateIdx:  /* fall through */
  case MethodpropertyCall_protectedIdx:  /* fall through */
  case MethodpropertyRedefine_protectedIdx:  /* fall through */
    {
      int impliedSetFlag = 0, impliedClearFlag = 0;

      switch (methodproperty) {
      case MethodpropertyClass_onlyIdx:
        flag = NSF_CMD_CLASS_ONLY_METHOD;
        break;
      case MethodpropertyCall_privateIdx:
        flag = NSF_CMD_CALL_PRIVATE_METHOD;
        impliedSetFlag = NSF_CMD_CALL_PROTECTED_METHOD;
        break;
      case MethodpropertyCall_protectedIdx:
        impliedClearFlag = NSF_CMD_CALL_PRIVATE_METHOD;
        flag = NSF_CMD_CALL_PROTECTED_METHOD;
        break;
      case MethodpropertyRedefine_protectedIdx:
        flag = NSF_CMD_REDEFINE_PROTECTED_METHOD;
        break;
      default: flag = 0;
      }

      if (valueObj != NULL) {
        int bool, result;

        result = Tcl_GetBooleanFromObj(interp, valueObj, &bool);
        if (unlikely(result != TCL_OK)) {
          return result;
        }
        if (bool != 0) {
          /*
           * set flag
           */
          Tcl_Command_flags(cmd) |= flag;
          if (impliedSetFlag != 0) {
            Tcl_Command_flags(cmd) |= impliedSetFlag;
          }
        } else {
          /*
           * clear flag
           */
          Tcl_Command_flags(cmd) &= ~flag;
          if (impliedClearFlag != 0) {
            Tcl_Command_flags(cmd) &= ~impliedClearFlag;
          }
        }
        if (cl != NULL) {
          NsfInstanceMethodEpochIncr("Permissions");
        } else {
          NsfObjectMethodEpochIncr("Permissions");
        }
      }
      Tcl_SetIntObj(Tcl_GetObjResult(interp), (Tcl_Command_flags(cmd) & flag) != 0);
      break;
    }
  case MethodpropertyReturnsIdx:
    {
      NsfParamDefs *paramDefs;
      Tcl_Obj **objPtr;

      paramDefs = ParamDefsGet(cmd, NULL);
      /*fprintf(stderr, "MethodProperty, ParamDefsGet cmd %p paramDefs %p returns %p\n",
        cmd, paramDefs, (paramDefs != NULL) ?paramDefs->returns:NULL);*/

      if (valueObj == NULL) {
        /* a query for "returns" */
        Tcl_Obj *resultObj;

        if (paramDefs == NULL) {
          resultObj = NsfGlobalObjs[NSF_EMPTY];
        } else {
          objPtr = &paramDefs->returns;
          resultObj = *objPtr ? *objPtr : NsfGlobalObjs[NSF_EMPTY];
        }
        Tcl_SetObjResult(interp, resultObj);

      } else {
        /* setting "returns" */
        const char *valueString = ObjStr(valueObj);

        if (paramDefs == NULL) {
          /* acquire new paramDefs */
          paramDefs = ParamDefsNew();
          ParamDefsStore(interp, cmd, paramDefs, 0);
          /*fprintf(stderr, "new param definitions %p for cmd %p %s\n", paramDefs, cmd, methodName);*/
        }

        objPtr = &paramDefs->returns;

        /* Set a new value; if there is already a value, free it */
        if (*objPtr) {
          DECR_REF_COUNT2("paramDefsObj", *objPtr);
        }
        if (*valueString == '\0') {
          /* set the value to NULL */
          *objPtr = NULL;
        } else {
          *objPtr = valueObj;
          INCR_REF_COUNT2("paramDefsObj", *objPtr);
        }
      }
      break;
    }
  }

  return TCL_OK;
}


/*
cmd "method::registered" NsfMethodRegisteredCmd {
  {-argName "handle" -required 1 -type tclobj}
}
*/
static int
NsfMethodRegisteredCmd(Tcl_Interp *interp, Tcl_Obj *methodNameObj) {
  NsfObject *regObject;
  int fromClassNS = 0;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodNameObj != NULL);

  cmd = ResolveMethodName(interp, NULL, methodNameObj,
                          NULL, &regObject, NULL, NULL, &fromClassNS);

  /*
   * In case the provided cmd is fully qualified and refers to a registered
   * method, the function returns the object, on which the method was
   * resisted.
   */
  Tcl_SetObjResult(interp, (cmd && regObject) ? regObject->cmdName : NsfGlobalObjs[NSF_EMPTY]);

  return TCL_OK;
}

/*
cmd method::setter NsfMethodSetterCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "parameter" -type tclobj}
  }
*/
static int
NsfMethodSetterCmd(Tcl_Interp *interp, NsfObject *object, int withPer_object, Tcl_Obj *parameter) {
  NsfClass *cl = (withPer_object || ! NsfObjectIsClass(object)) ? NULL : (NsfClass *)object;
  const char *methodName = ObjStr(parameter);
  SetterCmdClientData *setterClientData;
  size_t j, length;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(parameter != NULL);

  if (unlikely(*methodName == '-' || *methodName == ':')) {
    return NsfPrintError(interp, "invalid setter name \"%s\" (must not start with a dash or colon)",
                         methodName);
  }

  setterClientData = NEW(SetterCmdClientData);
  setterClientData->object = NULL;
  setterClientData->paramsPtr = NULL;

  length = strlen(methodName);

  for (j = 0; j < length; j++) {
    if (methodName[j] == ':' || methodName[j] == ' ') {
      break;
    }
  }

  if (j < length) {
    /* looks as if we have a parameter specification */
    int result, possibleUnknowns = 0, plainParams = 0, nrNonposArgs = 0;

    setterClientData->paramsPtr = ParamsNew(1);
    result = ParamParse(interp, NsfGlobalObjs[NSF_SETTER], parameter,
                        NSF_DISALLOWED_ARG_SETTER|NSF_ARG_HAS_DEFAULT,
                        setterClientData->paramsPtr, &possibleUnknowns,
                        &plainParams, &nrNonposArgs);

    if (unlikely(result != TCL_OK)) {
      SetterCmdDeleteProc(setterClientData);
      return result;
    }
    methodName = setterClientData->paramsPtr->name;
  } else {
    setterClientData->paramsPtr = NULL;
  }

  if (cl != NULL) {
    result = NsfAddClassMethod(interp, (Nsf_Class *)cl, methodName,
                               (Tcl_ObjCmdProc *)NsfSetterMethod,
                               setterClientData, SetterCmdDeleteProc, 0);
  } else {
    result = NsfAddObjectMethod(interp, (Nsf_Object *)object, methodName,
                                (Tcl_ObjCmdProc *)NsfSetterMethod,
                                setterClientData, SetterCmdDeleteProc, 0);
  }
  if (likely(result == TCL_OK)) {
    Tcl_SetObjResult(interp, MethodHandleObj(object, cl == NULL, methodName));
  } else {
    SetterCmdDeleteProc(setterClientData);
  }
  return result;
}

/*
cmd "object::alloc" NsfObjectAllocCmd {
  {-argName "class" -required 1 -type class}
  {-argName "name" -required 1 -type tclobj}
  {-argName "initcmd" -required 0 -type tclobj}
}
*/
static int
NsfObjectAllocCmd(Tcl_Interp *interp, NsfClass *class, Tcl_Obj *nameObj, Tcl_Obj *initcmdObj) {
  Tcl_Obj *newNameObj = NULL;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(nameObj != NULL);

  /*
   * If the provided name is empty, make a new symbol
   */
  if (strlen(ObjStr(nameObj)) == 0) {
    Tcl_DString ds, *dsPtr = &ds;

    Tcl_DStringInit(dsPtr);
    Tcl_DStringAppend(dsPtr, "::nsf::__#", 10);

    NewTclCommand(interp, dsPtr);

    newNameObj = Tcl_NewStringObj(Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));
    INCR_REF_COUNT(newNameObj);
    Tcl_DStringFree(dsPtr);

    nameObj = newNameObj;
  }

  /*fprintf(stderr, "trying to alloc <%s>\n", ObjStr(nameObj));*/

  result = NsfCAllocMethod(interp, class, nameObj);

  if (result == TCL_OK && initcmdObj) {
    NsfObject *object;
    Tcl_Obj *nameObj = Tcl_GetObjResult(interp);

    INCR_REF_COUNT(nameObj);
    if (unlikely(GetObjectFromObj(interp, nameObj, &object) != TCL_OK)) {
      return NsfPrintError(interp, "couldn't find result of alloc");
    }
    result = NsfDirectDispatchCmd(interp, object, 1,
                                  NsfGlobalObjs[NSF_EVAL],
                                  1, &initcmdObj);
    if (likely(result == TCL_OK)) {
      Tcl_SetObjResult(interp,nameObj);
    }

    DECR_REF_COUNT(nameObj);

  }

  if (newNameObj != NULL) {
    DECR_REF_COUNT(newNameObj);
  }

  return result;
}

/*
cmd "object::exists" NsfObjectExistsCmd {
  {-argName "value" -required 1 -type tclobj}
}
*/
static int
NsfObjectExistsCmd(Tcl_Interp *interp, Tcl_Obj *valueObj) {
  NsfObject *object;

  nonnull_assert(interp != NULL);
  nonnull_assert(valueObj != NULL);

  /*
   * Pass the object as Tcl_Obj, since we do not want to raise an error in
   * case the object does not exist.
   */
  Tcl_SetBooleanObj(Tcl_GetObjResult(interp), GetObjectFromObj(interp, valueObj, &object) == TCL_OK);
  return TCL_OK;
}

/*
cmd "object::property" NsfObjectPropertyCmd {
  {-argName "objectName" -required 1 -type object}
  {-argName "objectproperty" -type "initialized|class|rootmetaclass|rootclass|volatile|slotcontainer|hasperobjectslots|keepcallerself|perobjectdispatch" -required 1}
  {-argName "value" -required 0 -type tclobj}
}
*/

static int
NsfObjectPropertyCmd(Tcl_Interp *interp, NsfObject *object, int objectproperty, Tcl_Obj *valueObj) {
  unsigned int flags = 0u, allowSet = 0u;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  switch (objectproperty) {
  case ObjectpropertyInitializedIdx: flags = NSF_INIT_CALLED; allowSet = 1; break;
  case ObjectpropertyClassIdx: flags = NSF_IS_CLASS; break;
  case ObjectpropertyRootmetaclassIdx: flags = NSF_IS_ROOT_META_CLASS; break;
  case ObjectpropertyVolatileIdx:
    if (valueObj == NULL) {
      Tcl_SetObjResult(interp,
                       NsfGlobalObjs[object->opt != NULL && object->opt->volatileVarName ? NSF_ONE : NSF_ZERO]);
      return TCL_OK;
    };
    /* if value is provided, return the error below */
    break;
  case ObjectpropertyRootclassIdx: flags = NSF_IS_ROOT_CLASS; break;
  case ObjectpropertySlotcontainerIdx: flags = NSF_IS_SLOT_CONTAINER; allowSet = 1; break;
  case ObjectpropertyKeepcallerselfIdx: flags = NSF_KEEP_CALLER_SELF; allowSet = 1; break;
  case ObjectpropertyPerobjectdispatchIdx: flags = NSF_PER_OBJECT_DISPATCH; allowSet = 1; break;
  case ObjectpropertyHasperobjectslotsIdx: flags = NSF_HAS_PER_OBJECT_SLOTS; allowSet = 1; break;
  }

  if (valueObj != NULL) {
    if (likely(allowSet)) {
      int flagValue, result;

      result = SetBooleanFlag(interp, &object->flags, flags, valueObj, &flagValue);
      if (unlikely(result != TCL_OK)) {
        return result;
      }

      if (objectproperty == ObjectpropertySlotcontainerIdx) {
        assert(object->nsPtr != NULL);
        if (flagValue != 0) {
          /* turn on SlotContainerCmdResolver */
          Tcl_SetNamespaceResolvers(object->nsPtr,
                                    (Tcl_ResolveCmdProc *)SlotContainerCmdResolver,
                                    NsColonVarResolver,
                                    (Tcl_ResolveCompiledVarProc *)NULL);
        } else {
          /* turn off SlotContainerCmdResolver */
          Tcl_SetNamespaceResolvers(object->nsPtr,
                                    (Tcl_ResolveCmdProc *)NULL,
                                    NsColonVarResolver,
                                    (Tcl_ResolveCompiledVarProc *)NULL);
        }
      }

    } else {
      return NsfPrintError(interp, "object property is read only");
    }
  }

  Tcl_SetObjResult(interp,
                   NsfGlobalObjs[(object->flags & flags) ?
                                 NSF_ONE : NSF_ZERO]);
  return TCL_OK;
}

/*
cmd "object::qualify" NsfObjectQualifyCmd {
  {-argName "objectName" -required 1 -type tclobj}
}
*/
static int
NsfObjectQualifyCmd(Tcl_Interp *interp, Tcl_Obj *nameObj) {
  const char *nameString = ObjStr(nameObj);

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);

  if (isAbsolutePath(nameString)) {
    Tcl_SetObjResult(interp, nameObj);
  } else {
    Tcl_SetObjResult(interp, NameInNamespaceObj(nameString, CallingNameSpace(interp)));
  }
  return TCL_OK;
}

/*
cmd "objectsystem::create" NsfObjectSystemCreateCmd {
  {-argName "rootClass" -required 1 -type tclobj}
  {-argName "rootMetaClass" -required 1 -type tclobj}
  {-argName "systemMethods" -required 0 -type tclobj}
}
*/
static int
NsfObjectSystemCreateCmd(Tcl_Interp *interp, Tcl_Obj *Object, Tcl_Obj *Class, Tcl_Obj *systemMethodsObj) {
  NsfClass *theobj = NULL, *thecls = NULL;
  Tcl_Obj *object, *class;
  const char *objectName = ObjStr(Object);
  const char *className = ObjStr(Class);
  NsfObjectSystem *osPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(Object != NULL);
  nonnull_assert(Class != NULL);

  osPtr = NEW(NsfObjectSystem);
  memset(osPtr, 0, sizeof(NsfObjectSystem));

  object = isAbsolutePath(objectName) ? Object :
    NameInNamespaceObj(objectName, CallingNameSpace(interp));
  class = isAbsolutePath(className) ? Class :
    NameInNamespaceObj(className, CallingNameSpace(interp));

  GetClassFromObj(interp, object, &theobj, 0);
  GetClassFromObj(interp, class, &thecls, 0);

  if (theobj || thecls) {
    ObjectSystemFree(interp, osPtr);
    NsfLog(interp, NSF_LOG_WARN, "Base class '%s' exists already; ignoring definition",
           (theobj != NULL) ? objectName : className);
    return TCL_OK;
  }

  if (systemMethodsObj != NULL) {
    int oc, idx, result;
    Tcl_Obj **ov;

    if ((result = Tcl_ListObjGetElements(interp, systemMethodsObj, &oc, &ov)) == TCL_OK) {
      int i;

      if (oc % 2) {
        ObjectSystemFree(interp, osPtr);
        return NsfPrintError(interp, "system methods must be provided as pairs");
      }
      for (i = 0; i < oc; i += 2) {
        Tcl_Obj *arg, **arg_ov;
        int arg_oc = -1;

        arg = ov[i+1];
        result = Tcl_GetIndexFromObj(interp, ov[i], Nsf_SystemMethodOpts, "system method", 0, &idx);
        if (likely(result == TCL_OK)) {
          result = Tcl_ListObjGetElements(interp, arg, &arg_oc, &arg_ov);
        }
        if (unlikely(result != TCL_OK)) {
          ObjectSystemFree(interp, osPtr);
          return NsfPrintError(interp, "invalid system method '%s'", ObjStr(ov[i]));
        } else if (arg_oc < 1 || arg_oc > 3) {
          ObjectSystemFree(interp, osPtr);
          return NsfPrintError(interp, "invalid system method argument '%s'", ObjStr(ov[i]), ObjStr(arg));
        }
        /*fprintf(stderr, "NsfCreateObjectSystemCmd [%d] = %p %s (max %d, given %d)\n",
          idx, ov[i+1], ObjStr(ov[i+1]), NSF_s_set_idx, oc);*/

        if (arg_oc == 1) {
          osPtr->methods[idx] = arg;
        } else { /* (arg_oc == 2) */
          osPtr->methods[idx] = arg_ov[0];
          osPtr->handles[idx] = arg_ov[1];
          if  (arg_oc == 3) {
            int bool = 0;
            Tcl_GetBooleanFromObj(interp, arg_ov[2], &bool);
            osPtr->protected[idx] = bool;
          }
          INCR_REF_COUNT(osPtr->handles[idx]);
        }
        INCR_REF_COUNT(osPtr->methods[idx]);
      }
    } else {
      ObjectSystemFree(interp, osPtr);
      return NsfPrintError(interp, "provided system methods are not a proper list");
    }
  }
  /*
   * Create a basic object system with the basic root class Object and the
   * basic metaclass Class, and store them in the RUNTIME STATE if successful.
   */
  theobj = PrimitiveCCreate(interp, object, NULL, NULL);
  thecls = PrimitiveCCreate(interp, class, NULL, NULL);
  /* fprintf(stderr, "CreateObjectSystem created base classes \n"); */

  /* check whether Object and Class creation was successful */
  if (theobj == NULL || thecls == NULL) {

    if (thecls != NULL) {
      PrimitiveCDestroy(thecls);
    }
    if (theobj != NULL) {
      PrimitiveCDestroy(theobj);
    }

    ObjectSystemFree(interp, osPtr);
    return NsfPrintError(interp, "creation of object system failed");
  }

  theobj->osPtr = osPtr;
  thecls->osPtr = osPtr;
  osPtr->rootClass = theobj;
  osPtr->rootMetaClass = thecls;

  theobj->object.flags |= (NSF_IS_ROOT_CLASS|NSF_INIT_CALLED);
  thecls->object.flags |= (NSF_IS_ROOT_META_CLASS|NSF_INIT_CALLED);

  ObjectSystemAdd(interp, osPtr);

  AddInstance((NsfObject *)theobj, thecls);
  AddInstance((NsfObject *)thecls, thecls);
  AddSuper(thecls, theobj);

  if (NSF_DTRACE_OBJECT_ALLOC_ENABLED()) {
    NSF_DTRACE_OBJECT_ALLOC(ObjectName((NsfObject *)theobj), ClassName(((NsfObject *)theobj)->cl));
    NSF_DTRACE_OBJECT_ALLOC(ObjectName((NsfObject *)thecls), ClassName(((NsfObject *)thecls)->cl));
  }

  return TCL_OK;
}


/*
cmd my NsfMyCmd {
  {-argName "-intrinsic" -nrargs 0}
  {-argName "-local" -nrargs 0}
  {-argName "-system" -nrargs 0}
  {-argName "method" -required 1 -type tclobj}
  {-argName "args" -type args}
}
*/
static int
NsfMyCmd(Tcl_Interp *interp,
         int withIntrinsic, int withLocal, int withSystem,
         Tcl_Obj *methodObj, int nobjc, Tcl_Obj *CONST nobjv[]) {
  NsfObject *self = GetSelfObj(interp);
  unsigned int flags;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(methodObj != NULL);

  if (unlikely(self == NULL)) {
    return NsfNoCurrentObjectError(interp, method_definitions[NsfMyCmdIdx].methodName);
  }

  if ((withIntrinsic && withLocal)
      || (withIntrinsic && withSystem)
      || (withLocal && withSystem)) {
    return NsfPrintError(interp, "flags '-intrinsic', '-local' and '-system' are mutual exclusive");
  }

#if 0
  /* TODO attempt to make "my" NRE-enabled, failed so far (crash in mixinInheritanceTest) */
  NsfCallStackContent *cscPtr = CallStackGetTopFrame0(interp);
  if (cscPtr == NULL || self != cscPtr->self) {
    flags = NSF_CSC_IMMEDIATE;
  } else {
    flags = NsfImmediateFromCallerFlags(cscPtr->flags);
    fprintf(stderr, "XXX MY %s.%s frame has flags %.6x -> next-flags %.6x\n",
            ObjectName(self), ObjStr(methodObj), cscPtr->flags, flags);
  }
  if (withIntrinsic != 0) {flags |= NSF_CM_INTRINSIC_METHOD;}
  if (withLocal != 0)     {flags |= NSF_CM_LOCAL_METHOD;}
  if (withSystem != 0)    {flags |= NSF_CM_SYSTEM_METHOD;}
  result = CallMethod(self, interp, methodObj, nobjc+2, nobjv, flags);
#else
  flags = NSF_CSC_IMMEDIATE;
  if (withIntrinsic != 0) {flags |= NSF_CM_INTRINSIC_METHOD;}
  if (withLocal != 0)     {flags |= NSF_CM_LOCAL_METHOD;}
  if (withSystem != 0)    {flags |= NSF_CM_SYSTEM_METHOD;}
  result = CallMethod(self, interp, methodObj, nobjc+2, nobjv, flags);
#endif

  return result;
}


/*
 *----------------------------------------------------------------------
 * NsfNextCmd --
 *
 *    nsf::next calls the next shadowed method. It might get a single
 *    argument which is used as argument vector for that method. If no
 *    argument is provided, the argument vector of the last invocation
 *    is used.
 *
 * Results:
 *    Tcl return code
 *
 * Side effects:
 *    The invoked method might produce side effects
 *
 *----------------------------------------------------------------------
 */
/*
cmd next NsfNextCmd {
  {-argName "arguments" -required 0 -type tclobj}
}
*/
static int
NsfNextCmd(Tcl_Interp *interp, Tcl_Obj *arguments) {
  int freeArgumentVector, oc, nobjc = 0, result;
  NsfCallStackContent *cscPtr = NULL;
  const char *methodName = NULL;
  Tcl_Obj **nobjv = NULL, **ov;

  nonnull_assert(interp != NULL);

  if (arguments != NULL) {
    /* Arguments were provided. */
    int result = Tcl_ListObjGetElements(interp, arguments, &oc, &ov);
    if (unlikely(result != TCL_OK)) {return result;}
  } else {
    /* No arguments were provided. */
    oc = -1;
    ov = NULL;
  }

  result = NextGetArguments(interp, oc, ov, &cscPtr, &methodName,
                            &nobjc, &nobjv, &freeArgumentVector);
  if (likely(result == TCL_OK)) {
    assert(cscPtr != NULL);
    result = NextSearchAndInvoke(interp, methodName, nobjc, nobjv, cscPtr, freeArgumentVector);
  }
  return result;
}

/*
cmd nscopyvars NsfNSCopyVars {
  {-argName "fromNs" -required 1 -type tclobj}
  {-argName "toNs" -required 1 -type tclobj}
}
*/
static int
NsfNSCopyVarsCmd(Tcl_Interp *interp, Tcl_Obj *fromNs, Tcl_Obj *toNs) {
  Tcl_Namespace *fromNsPtr = NULL, *toNsPtr;
  Var *varPtr = NULL;
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;
  TclVarHashTable *varTablePtr;
  NsfObject *object, *destObject;
  const char *destFullName;
  Tcl_Obj *destFullNameObj;
  Tcl_CallFrame frame, *framePtr = &frame;
  Tcl_Obj *varNameObj = NULL;
  int      result;

  nonnull_assert(interp != NULL);
  nonnull_assert(fromNs != NULL);
  nonnull_assert(toNs != NULL);

  TclGetNamespaceFromObj(interp, fromNs, &fromNsPtr);

  if (fromNsPtr != NULL) {
    if (TclGetNamespaceFromObj(interp, toNs, &toNsPtr) != TCL_OK) {
      return NsfPrintError(interp, "CopyVars: Destination namespace %s does not exist",
                           ObjStr(toNs));
    }

    object = GetObjectFromString(interp, ObjStr(fromNs));
    destFullName = toNsPtr->fullName;
    destFullNameObj = Tcl_NewStringObj(destFullName, -1);
    INCR_REF_COUNT(destFullNameObj);
    varTablePtr = Tcl_Namespace_varTablePtr(fromNsPtr);
    Tcl_PushCallFrame(interp, (Tcl_CallFrame *)framePtr, toNsPtr, 0);
  } else {
    NsfObject *newObject;

    if (GetObjectFromObj(interp, fromNs, &object) != TCL_OK) {
      return NsfPrintError(interp, "CopyVars: Origin object/namespace %s does not exist",
                           ObjStr(fromNs));
    }
    if (GetObjectFromObj(interp, toNs, &newObject) != TCL_OK) {
      return NsfPrintError(interp, "CopyVars: Destination object/namespace %s does not exist",
                           ObjStr(toNs));
    }
    varTablePtr = object->varTablePtr;
    destFullNameObj = newObject->cmdName;
    destFullName = ObjStr(destFullNameObj);
  }

  destObject = GetObjectFromString(interp, destFullName);
  result = TCL_OK;

  /* copy all vars in the ns */
  hPtr = (varTablePtr != NULL) ? Tcl_FirstHashEntry(TclVarHashTablePtr(varTablePtr), &hSrch) : NULL;
  while (hPtr) {
    Tcl_Obj *resultObj;

    GetVarAndNameFromHash(hPtr, &varPtr, &varNameObj);
    INCR_REF_COUNT(varNameObj);

    if (!TclIsVarUndefined(varPtr) && !TclIsVarLink(varPtr)) {
      if (TclIsVarScalar(varPtr)) {
        /*
         * Copy scalar variables from the namespace, which might be
         * either object or namespace variables.
         */

        if (object != NULL) {
          /* fprintf(stderr, "copy in obj %s var %s val '%s'\n", ObjectName(object), ObjStr(varNameObj),
             ObjStr(TclVarValue(Tcl_Obj, varPtr, objPtr)));*/

          resultObj = Nsf_ObjSetVar2((Nsf_Object *)destObject, interp, varNameObj, NULL,
                                     TclVarValue(Tcl_Obj, varPtr, objPtr), TCL_LEAVE_ERR_MSG);
        } else {
          resultObj = Tcl_ObjSetVar2(interp, varNameObj, NULL,
                                     TclVarValue(Tcl_Obj, varPtr, objPtr),
                                     TCL_NAMESPACE_ONLY|TCL_LEAVE_ERR_MSG);
        }
        if (unlikely(resultObj == NULL)) {
          DECR_REF_COUNT(varNameObj);
          result = TCL_ERROR;
          goto copy_done;
        }
      } else {
        if (TclIsVarArray(varPtr)) {
          /* HERE!! PRE85 Why not [array get/set] based? Let the core iterate */
          TclVarHashTable *aTable = TclVarValue(TclVarHashTable, varPtr, tablePtr);
          Tcl_HashSearch ahSrch;
          Tcl_HashEntry *ahPtr = (aTable != NULL) ? Tcl_FirstHashEntry(TclVarHashTablePtr(aTable), &ahSrch) : 0;

          for (; ahPtr; ahPtr = Tcl_NextHashEntry(&ahSrch)) {
            Tcl_Obj *eltNameObj;
            Var *eltVar;

            GetVarAndNameFromHash(ahPtr, &eltVar, &eltNameObj);
            INCR_REF_COUNT(eltNameObj);

            if (TclIsVarScalar(eltVar)) {
              if (object != NULL) {
                resultObj = Nsf_ObjSetVar2((Nsf_Object *)destObject, interp, varNameObj, eltNameObj,
                                           TclVarValue(Tcl_Obj, eltVar, objPtr), TCL_LEAVE_ERR_MSG);
              } else {
                resultObj = Tcl_ObjSetVar2(interp, varNameObj, eltNameObj,
                                           TclVarValue(Tcl_Obj, eltVar, objPtr),
                                           TCL_NAMESPACE_ONLY|TCL_LEAVE_ERR_MSG);
              }
              if (unlikely(resultObj == NULL)) {
                DECR_REF_COUNT(varNameObj);
                result = TCL_ERROR;
                goto copy_done;
              }
            }
            DECR_REF_COUNT(eltNameObj);
          }
        }
      }
    }
    DECR_REF_COUNT(varNameObj);
    hPtr = Tcl_NextHashEntry(&hSrch);
  }
 copy_done:
  if (fromNsPtr != NULL) {
    DECR_REF_COUNT(destFullNameObj);
    Tcl_PopCallFrame(interp);
  }
  return result;
}

/*
cmd parameter::info NsfParameterInfoCmd {
  {-argName "parametersubcmd" -type "default|list|name|syntax|type" -required 1}
  {-argName "parameterspec"   -required 1 -type tclobj}
  {-argName "varname"         -required 0 -type tclobj}
}
*/
static int
NsfParameterInfoCmd(Tcl_Interp *interp, int parametersubcmd, Tcl_Obj *parameterspec,  Tcl_Obj *varname) {
  NsfParsedParam parsedParam;
  Tcl_Obj *paramsObj, *listObj = NULL;
  Nsf_Param *paramsPtr;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(parameterspec != NULL);

  if (parametersubcmd != ParametersubcmdDefaultIdx && varname != NULL) {
    return NsfPrintError(interp, "parameter::info: provided third argument is only valid for querying defaults");
  }

  paramsObj = Tcl_NewListObj(1, &parameterspec);
  INCR_REF_COUNT(paramsObj);
  result = ParamDefsParse(interp, NULL, paramsObj,
                            NSF_DISALLOWED_ARG_OBJECT_PARAMETER, 1,
                            &parsedParam);
  DECR_REF_COUNT(paramsObj);

  if (unlikely(result != TCL_OK)) {
    return result;
  }

  assert(parsedParam.paramDefs);
  paramsPtr = parsedParam.paramDefs->paramsPtr;
  assert(paramsPtr != NULL);

  /*
   * Since we are passing in a parameter definition in Tcl syntax, and we want
   * to extract information from that syntax, it make limited sense to provide
   * a context object for virutal parameter expansion. At least, we do not
   * allow this so far.
   */

  switch (parametersubcmd) {
  case ParametersubcmdDefaultIdx:
    if (paramsPtr->defaultValue != NULL) {
      if (varname != NULL) {
        Tcl_Obj *resultObj = Tcl_ObjSetVar2(interp, varname, NULL,
                                            paramsPtr->defaultValue,
                                            TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1);
        if (unlikely(resultObj == NULL)) {
          ParamDefsRefCountDecr(parsedParam.paramDefs);
          return TCL_ERROR;
        }
      }
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_ONE]);
    } else {
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_ZERO]);
    }
    break;

  case ParametersubcmdListIdx:
    listObj = ParamDefsList(interp, paramsPtr, NULL, NULL);
    Tcl_SetObjResult(interp, listObj);
    DECR_REF_COUNT2("paramDefsObj", listObj);
    break;

  case ParametersubcmdNameIdx:
    listObj = ParamDefsNames(interp, paramsPtr, NULL, NULL);
    Tcl_SetObjResult(interp, listObj);
    DECR_REF_COUNT2("paramDefsObj", listObj);
    break;

  case ParametersubcmdSyntaxIdx:
    listObj = NsfParamDefsSyntax(interp, paramsPtr, NULL, NULL);
    Tcl_SetObjResult(interp, listObj);
    DECR_REF_COUNT2("paramDefsObj", listObj);
    break;

  case ParametersubcmdTypeIdx:
    if (paramsPtr->type != NULL) {

      if (paramsPtr->converter == Nsf_ConvertToTclobj && paramsPtr->converterArg) {
        Tcl_SetObjResult(interp, paramsPtr->converterArg);

      } else {
        if (paramsPtr->converter == Nsf_ConvertToObject || paramsPtr->converter == Nsf_ConvertToClass) {
          const char *what = paramsPtr->type;
          /*
           * baseclass and metaclass are communicated via flags
           */
          if (unlikely((paramsPtr->flags & NSF_ARG_BASECLASS) != 0u)) {
            what = "baseclass";
          } else if (unlikely((paramsPtr->flags & NSF_ARG_METACLASS) != 0u)) {
            what = "metaclass";
          }
          /*
           * The converterArg might contain a class for type checking
           */
          if (paramsPtr->converterArg == NULL) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(what, -1));
          } else {
            Tcl_SetObjResult(interp, paramsPtr->converterArg);
          }
        } else {
          Tcl_SetObjResult(interp, Tcl_NewStringObj(paramsPtr->type, -1));
        }
      }
    } else {
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_EMPTY]);
    }
    break;
  }

  ParamDefsRefCountDecr(parsedParam.paramDefs);

  return TCL_OK;
}

/*
cmd parameter::cache::classinvalidate NsfParameterCacheClassInvalidateCmd {
  {-argName "class" -required 1 -type class}
}
*/
static int
NsfParameterCacheClassInvalidateCmd(Tcl_Interp *interp, NsfClass *cl) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  /*
   * First, increment the epoch in case we have a parsedParam. The
   * classParamPtrEpoch is just used for PER_OBJECT_PARAMETER_CACHING
   */
#if defined(PER_OBJECT_PARAMETER_CACHING)
  if (unlikely(cl->parsedParamPtr != NULL)) {
    NsfClassParamPtrEpochIncr("NsfParameterCacheClassInvalidateCmd");
  }
#endif

  /*
   * During shutdown, no new objects are created, therefore we do not need to
   * to invalidate the cached parsedParamPtr of the classes.
   */
  if (unlikely(RUNTIME_STATE(interp)->exitHandlerDestroyRound == NSF_EXITHANDLER_OFF)) {
    NsfClasses *dependentSubClasses;
    NsfClasses *clPtr;

    /*
     * Clear the cached parsedParam of the class and all its subclasses (the
     * result of DependentSubClasses() contains the starting class). Furthermore,
     * make a quick check, if any of the subclasses is a class mixin of some
     * other class.
     */

    dependentSubClasses = DependentSubClasses(cl);

    if (dependentSubClasses != NULL) {

      for (clPtr = dependentSubClasses; clPtr; clPtr = clPtr->nextPtr) {
        NsfClass *subClass = clPtr->cl;

        if (subClass->parsedParamPtr != NULL) {
          ParsedParamFree(subClass->parsedParamPtr);
          subClass->parsedParamPtr = NULL;
        }

      }
      NsfClassListFree(dependentSubClasses);
    }

  }
  return TCL_OK;
}


/*
cmd parameter::cache::objectinvalidate NsfParameterCacheObjectInvalidateCmd {
  {-argName "object" -required 1 -type object}
}
*/
static int
NsfParameterCacheObjectInvalidateCmd(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#if defined(PER_OBJECT_PARAMETER_CACHING)
  if (object->opt != NULL && object->opt->parsedParamPtr) {
    /*fprintf(stderr, "   %p %s invalidate %p\n", object,
      ObjectName(object),  object->opt->parsedParamPtr);*/
    ParsedParamFree(object->opt->parsedParamPtr);
    object->opt->parsedParamPtr = NULL;
  }
#endif
  return TCL_OK;
}

/*
cmd parameter::specs NsfParameterSpecsCmd {
  {-argName "-configure"  -nrargs 0 -required 0}
  {-argName "-nonposargs"  -nrargs 0 -required 0}
  {-argName "slotobjs"    -required 1 -type tclobj}
}
*/

static int
NsfParameterSpecsCmd(Tcl_Interp *interp, int withConfigure, int withNonposargs, Tcl_Obj *slotListObj) {
  NsfTclObjList *objList = NULL, *elt;
  Tcl_Obj **objv, *resultObj;
  int result = TCL_OK, i, objc;

  nonnull_assert(interp != NULL);
  nonnull_assert(slotListObj != NULL);

  if (Tcl_ListObjGetElements(interp, slotListObj, &objc, &objv) != TCL_OK) {
    return NsfPrintError(interp, "NsfParameterSpecsCmd: invalid slot object list");
  }

  /*
   * Iterate over the slot objects and obtain the position and the
   * parameterSpec.
   */
  for (i = 0; i < objc; i++) {
    NsfObject *slotObject;
    Tcl_Obj *positionObj, *specObj = NULL;

    if (GetObjectFromObj(interp, objv[i], &slotObject) != TCL_OK) {
      return NsfPrintError(interp, "objectparameter: slot element is not a next scripting object");
    }
    /*
     * When withConfigure is provided, skip this parameter ...
     *  - when configure is not set
     *  - or  configure == 0
     */
    if (withConfigure != 0) {
      int configure = 0;
      Tcl_Obj *configureObj = Nsf_ObjGetVar2((Nsf_Object *)slotObject, interp,
                                             NsfGlobalObjs[NSF_CONFIGURABLE], NULL, 0);
      if (configureObj == NULL) {
        continue;
      }
      Tcl_GetBooleanFromObj(interp, configureObj, &configure);
      if (configure == 0) {
        continue;
      }
    }

    /*
     * When withNonposargs is provided, skip this parameter ...
     *  - when positional == 1
     */
    if (withNonposargs != 0) {
      Tcl_Obj *positionalObj = Nsf_ObjGetVar2((Nsf_Object *)slotObject, interp,
                                             NsfGlobalObjs[NSF_POSITIONAL], NULL, 0);
      if (positionalObj != NULL) {
        int positional = 0;
        Tcl_GetBooleanFromObj(interp, positionalObj, &positional);
        if (positional != 0) {
          continue;
        }
      }
    }

    positionObj = Nsf_ObjGetVar2((Nsf_Object *)slotObject, interp,
                                 NsfGlobalObjs[NSF_POSITION], NULL, 0);
    specObj = Nsf_ObjGetVar2((Nsf_Object *)slotObject, interp,
                             NsfGlobalObjs[NSF_PARAMETERSPEC], NULL, 0);
    if (specObj == NULL) {
      result = CallMethod(slotObject, interp, NsfGlobalObjs[NSF_GET_PARAMETER_SPEC], 2, NULL,
                          NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE);
      if (unlikely(result != TCL_OK)) {
        return NsfPrintError(interp, "objectparameter: %s %s returned error",
                             ObjectName(slotObject),
                             NsfGlobalStrings[NSF_GET_PARAMETER_SPEC]);
      }
      specObj = Tcl_GetObjResult(interp);
    }
    /*fprintf(stderr, "NsfParameterSpecsCmd  slot obj = %s pos %s spec %s\n", ObjStr(objv[i]), (positionObj != NULL) ? ObjStr(positionObj) : "NONE", ObjStr(specObj)  );*/
    /*
     * Add the spec to the list indicated by the position
     */
    TclObjListAdd(interp, &objList, positionObj, specObj);
  }

  /*
   * Fold the per-position lists into a flat result list
   */
  resultObj = Tcl_NewListObj(0, NULL);
  for (elt = objList; elt; elt = elt->nextPtr) {
    Tcl_ListObjGetElements(interp, elt->payload, &objc, &objv);
    for (i = 0; i < objc; i++) {
      Tcl_ListObjAppendElement(interp, resultObj, objv[i]);

    }
  }

  Tcl_SetObjResult(interp, resultObj);
  if (objList != NULL) {
    TclObjListFreeList(objList);
  }

  return result;
}

/*
cmd proc NsfProcCmd {
  {-argName "-ad" -required 0  -nrargs 0 -type switch}
  {-argName "-checkalways" -required 0  -nrargs 0 -type switch}
  {-argName "procName" -required 1 -type tclobj}
  {-argName "arguments" -required 1 -type tclobj}
  {-argName "body" -required 1 -type tclobj}
}
*/
static int
NsfProcCmd(Tcl_Interp *interp, int with_ad, int with_checkAlways, Tcl_Obj *nameObj, Tcl_Obj *arguments, Tcl_Obj *body) {
  NsfParsedParam parsedParam;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(nameObj != NULL);
  nonnull_assert(arguments != NULL);
  nonnull_assert(body != NULL);

  /*
   * Parse argument list "arguments" to determine if we should provide
   * nsf parameter handling.
   */
  result = ParamDefsParse(interp, nameObj, arguments,
                          NSF_DISALLOWED_ARG_METHOD_PARAMETER, 0,
                          &parsedParam);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  if (parsedParam.paramDefs != NULL) {
    /*
     * We need parameter handling. In such cases, a thin C-based layer
     * is added which handles the parameter passing and calls the proc
     * later.
     */
    result = NsfProcAdd(interp, &parsedParam, ObjStr(nameObj), body, with_ad, with_checkAlways);

  } else {
    /*
     * No parameter handling needed. A plain Tcl proc is added.
     */
    Tcl_Obj *ov[4];

    ov[0] = NULL;
    ov[1] = nameObj;
    ov[2] = arguments;
    ov[3] = body;
    result = Tcl_ProcObjCmd(0, interp, 4, ov);
  }

  return result;
}

/*
cmd relation::get NsfRelationGetCmd {
  {-argName "object" -type object}
  {-argName "relationtype" -required 1 -type "object-mixin|class-mixin|object-filter|class-filter|class|superclass|rootclass"}
}
*/
static int
NsfRelationGetCmd(Tcl_Interp *interp, NsfObject *object, int relationtype) {

  return NsfRelationSetCmd(interp, object, relationtype, NULL);
}


/*
 *----------------------------------------------------------------------
 * NsfRelationClassMixinsSet --
 *
 *    Set class mixins; the main reason for the factored out semantics is that
 *    it can allow to undo/redo the operations in case of a failure.
 *
 * Results:
 *    Tcl result code.
 *
 * Side effects:
 *    class mixins are set, various kinds of invlidations.
 *
 *----------------------------------------------------------------------
 */
static int NsfRelationClassMixinsSet(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *valueObj, int oc, Tcl_Obj **ov)
  nonnull(1) nonnull(2) nonnull(3);

static int
NsfRelationClassMixinsSet(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *valueObj, int oc, Tcl_Obj **ov) {
  NsfCmdList *newMixinCmdList = NULL, *cmds;
  NsfClasses *subClasses;
  NsfClassOpt *clopt;
  int i;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(valueObj != NULL);

  for (i = 0; i < oc; i++) {
    if (unlikely(MixinAdd(interp, &newMixinCmdList, ov[i], cl->object.cl) != TCL_OK)) {
      CmdListFree(&newMixinCmdList, GuardDel);
      return TCL_ERROR;
    }
  }
  clopt = cl->opt;
  assert(clopt != NULL);

  if (clopt->classMixins != NULL) {
    RemoveFromClassMixinsOf(cl->object.id, clopt->classMixins);
    CmdListFree(&clopt->classMixins, GuardDel);
  }

  subClasses = DependentSubClasses(cl);
  MixinInvalidateObjOrders(interp, cl, subClasses);

  /*
   * Since methods of mixed in classes may be used as filters, we have to
   * invalidate the filters as well.
   */
  if (FiltersDefined(interp) > 0) {
    FilterInvalidateObjOrders(interp, subClasses);
  }
  NsfClassListFree(subClasses);

  /*
   * Now register the specified mixins.
   */
  clopt->classMixins = newMixinCmdList;

  /*
   * Finally, update classMixinOfs
   */
  for (cmds = newMixinCmdList; cmds; cmds = cmds->nextPtr) {
    NsfObject *nObject = NsfGetObjectFromCmdPtr(cmds->cmdPtr);
    if (nObject != NULL) {
      NsfClassOpt *nclopt = NsfRequireClassOpt((NsfClass *) nObject);
      CmdListAddSorted(&nclopt->isClassMixinOf, cl->object.id, NULL);
    } else {
      NsfLog(interp, NSF_LOG_WARN,
             "Problem registering %s as a class mixin of %s\n",
             ObjStr(valueObj), ClassName(cl));
    }
  }

  return TCL_OK;
}


/*
cmd relation::set NsfRelationSetCmd {
  {-argName "object" -type object}
  {-argName "relationtype" -required 1 -type "object-mixin|class-mixin|object-filter|class-filter|class|superclass|rootclass"}
  {-argName "value" -required 0 -type tclobj}
}
*/
static int
NsfRelationSetCmd(Tcl_Interp *interp, NsfObject *object,
                  int relationtype, Tcl_Obj *valueObj) {
  int oc; Tcl_Obj **ov;
  NsfClass *cl = NULL;
  NsfObjectOpt *objopt = NULL;
  NsfClassOpt *clopt = NULL, *nclopt = NULL;
  int i;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*fprintf(stderr, "NsfRelationSetCmd %s rel=%d val='%s'\n",
    ObjectName(object), relationtype, (valueObj != NULL) ? ObjStr(valueObj) : "NULL");*/

  if (relationtype == RelationtypeClass_mixinIdx ||
      relationtype == RelationtypeClass_filterIdx) {
    if (NsfObjectIsClass(object)) {
      cl = (NsfClass *)object;
    } else {
      /* fall back to per-object case */
      relationtype = (relationtype == RelationtypeClass_mixinIdx) ?
        RelationtypeObject_mixinIdx :
        RelationtypeObject_filterIdx ;
    }
  }

  switch (relationtype) {
  case RelationtypeObject_filterIdx:
  case RelationtypeObject_mixinIdx:
    if (valueObj == NULL) {
      objopt = object->opt;
      switch (relationtype) {
      case RelationtypeObject_mixinIdx:
        return (objopt != NULL) ? MixinInfo(interp, objopt->objMixins, NULL, 1, NULL) : TCL_OK;
      case RelationtypeObject_filterIdx:
        return (objopt != NULL) ? FilterInfo(interp, objopt->objFilters, NULL, 1, 0) : TCL_OK;
      }
    }
    if (unlikely(Tcl_ListObjGetElements(interp, valueObj, &oc, &ov) != TCL_OK)) {
      return TCL_ERROR;
    }
    objopt = NsfRequireObjectOpt(object);
    break;

  case RelationtypeClass_mixinIdx:
  case RelationtypeClass_filterIdx:

    if (valueObj == NULL) {
      clopt = cl->opt;
      switch (relationtype) {
      case RelationtypeClass_mixinIdx:
        return (clopt != NULL) ? MixinInfo(interp, clopt->classMixins, NULL, 1, NULL) : TCL_OK;
      case RelationtypeClass_filterIdx:
        return (clopt != NULL) ? FilterInfo(interp, clopt->classFilters, NULL, 1, 0) : TCL_OK;
      }
    }

    if (unlikely(Tcl_ListObjGetElements(interp, valueObj, &oc, &ov) != TCL_OK)) {
      return TCL_ERROR;
    }
    clopt = NsfRequireClassOpt(cl);
    break;

  case RelationtypeSuperclassIdx:
    if (!NsfObjectIsClass(object)) {
      return NsfObjErrType(interp, "superclass", object->cmdName, "class", NULL);
    }
    cl = (NsfClass *)object;
    if (valueObj == NULL) {
      return ListSuperClasses(interp, cl, NULL, 0);
    }
    if (unlikely(Tcl_ListObjGetElements(interp, valueObj, &oc, &ov) != TCL_OK)) {
      return TCL_ERROR;
    }
    return SuperclassAdd(interp, cl, oc, ov, valueObj);

  case RelationtypeClassIdx:
    if (valueObj == NULL) {
      Tcl_SetObjResult(interp, object->cl->object.cmdName);
      return TCL_OK;
    }
    GetClassFromObj(interp, valueObj, &cl, 1);
    if (cl == NULL) {
      return NsfObjErrType(interp, "class", valueObj, "a class", NULL);
    }
    i = ChangeClass(interp, object, cl);
    if (i == TCL_OK) {
      Tcl_SetObjResult(interp, object->cl->object.cmdName);
    }
    return i;

  case RelationtypeRootclassIdx:
    {
    NsfClass *metaClass = NULL;

    if (!NsfObjectIsClass(object)) {
      return NsfObjErrType(interp, "rootclass", object->cmdName, "class", NULL);
    }
    cl = (NsfClass *)object;

    if (valueObj == NULL) {
      return NsfPrintError(interp, "metaclass must be specified as third argument");
    }
    GetClassFromObj(interp, valueObj, &metaClass, 0);
    if (metaClass == NULL) {
      return NsfObjErrType(interp, "rootclass", valueObj, "class", NULL);
    }

    cl->object.flags |= NSF_IS_ROOT_CLASS;
    metaClass->object.flags |= NSF_IS_ROOT_META_CLASS;

    return TCL_OK;

    /* todo:
       need to remove these properties?
       allow to delete a class system at runtime?
    */
    }
  }

  switch (relationtype) {
  case RelationtypeObject_mixinIdx:
    {
      NsfCmdList *newMixinCmdList = NULL, *cmds;

      /*
       * Add every mixin class
       */
      for (i = 0; i < oc; i++) {
        if (unlikely(MixinAdd(interp, &newMixinCmdList, ov[i], object->cl->object.cl) != TCL_OK)) {
          CmdListFree(&newMixinCmdList, GuardDel);
          return TCL_ERROR;
        }
      }

      if (objopt->objMixins != NULL) {
        NsfCmdList *cmdlist, *del;

        /*
         * Delete from old isObjectMixinOf lists
         */
        for (cmdlist = objopt->objMixins; cmdlist; cmdlist = cmdlist->nextPtr) {
          cl = NsfGetClassFromCmdPtr(cmdlist->cmdPtr);
          clopt = (cl != NULL) ? cl->opt : NULL;
          if (clopt != NULL) {
            del = CmdListFindCmdInList(object->id, clopt->isObjectMixinOf);
            if (del != NULL) {
              /* fprintf(stderr, "Removing object %s from isObjectMixinOf of class %s\n",
                 ObjectName(object), ObjStr(NsfGetClassFromCmdPtr(cmdlist->cmdPtr)->object.cmdName)); */
              del = CmdListRemoveFromList(&clopt->isObjectMixinOf, del);
              CmdListDeleteCmdListEntry(del, GuardDel);
            }
          }
        }
        CmdListFree(&objopt->objMixins, GuardDel);
      }

      /*
       * Invalidate per-object infos
       */
      NsfParameterCacheObjectInvalidateCmd(interp, object);
      object->flags &= ~NSF_MIXIN_ORDER_VALID;
      /*
       * Since mixin procs may be used as filters -> we have to invalidate
       * filters as well.
       */
      object->flags &= ~NSF_FILTER_ORDER_VALID;

      /*
       * Now register the specified mixins.
       */
      objopt->objMixins = newMixinCmdList;

      for (cmds = newMixinCmdList; cmds; cmds = cmds->nextPtr) {
        NsfObject *nObject = NsfGetObjectFromCmdPtr(cmds->cmdPtr);

        if (nObject != NULL) {
          nclopt = NsfRequireClassOpt((NsfClass *) nObject);
          CmdListAddSorted(&nclopt->isObjectMixinOf, object->id, NULL);
        } else {
          NsfLog(interp, NSF_LOG_WARN,
                 "Problem registering %s as a object mixin of %s\n",
                 ObjStr(valueObj), ObjectName(object));
        }
      }

      MixinComputeDefined(interp, object);
      FilterComputeDefined(interp, object);

      break;
    }

  case RelationtypeObject_filterIdx:
    {
      NsfCmdList *newFilterCmdList = NULL;

      for (i = 0; i < oc; i ++) {
        if (unlikely(FilterAdd(interp, &newFilterCmdList, ov[i], object, NULL) != TCL_OK)) {
          CmdListFree(&newFilterCmdList, GuardDel);
          return TCL_ERROR;
        }
      }

      if (objopt->objFilters != NULL) {
        CmdListFree(&objopt->objFilters, GuardDel);
      }

      object->flags &= ~NSF_FILTER_ORDER_VALID;
      objopt->objFilters = newFilterCmdList;

      /*FilterComputeDefined(interp, object);*/
      break;
    }

  case RelationtypeClass_mixinIdx:
    if (unlikely(NsfRelationClassMixinsSet(interp, cl, valueObj, oc, ov) != TCL_OK)) {
      return TCL_ERROR;
    }
    break;

  case RelationtypeClass_filterIdx:
    {
      NsfCmdList *newFilterCmdList = NULL;

      for (i = 0; i < oc; i ++) {
        if (unlikely(FilterAdd(interp, &newFilterCmdList, ov[i], NULL, cl) != TCL_OK)) {
          CmdListFree(&newFilterCmdList, GuardDel);
          return TCL_ERROR;
        }
      }

      if (clopt->classFilters != NULL) {
        CmdListFree(&clopt->classFilters, GuardDel);
      }

      if (FiltersDefined(interp) > 0) {
        NsfClasses *subClasses = DependentSubClasses(cl);
        if (subClasses != NULL) {
          FilterInvalidateObjOrders(interp, subClasses);
          NsfClassListFree(subClasses);
        }
      }

      clopt->classFilters = newFilterCmdList;

      break;
    }
  }

  /*
   * Return on success the final setting
   */
  NsfRelationSetCmd(interp, object, relationtype, NULL);
  return TCL_OK;
}

/*
cmd current NsfCurrentCmd {
  {-argName "currentoption" -required 0 -type "proc|method|methodpath|object|class|activelevel|args|activemixin|calledproc|calledmethod|calledclass|callingproc|callingmethod|callingclass|callinglevel|callingobject|filterreg|isnextcall|next"}
}
*/
static int
NsfCurrentCmd(Tcl_Interp *interp, int selfoption) {
  NsfObject *object = GetSelfObj(interp);
  NsfCallStackContent *cscPtr;
  Tcl_CallFrame *framePtr;
  int result = TCL_OK;

  nonnull_assert(interp != NULL);

  if (selfoption == 0 || selfoption == CurrentoptionObjectIdx) {
    if (likely(object != NULL)) {
      Tcl_SetObjResult(interp, object->cmdName);
      return TCL_OK;
    } else {
      return NsfNoCurrentObjectError(interp, NULL);
    }
  }

  if (unlikely(object == NULL && selfoption != CurrentoptionCallinglevelIdx)) {
    return NsfNoCurrentObjectError(interp, NULL);
  }

  switch (selfoption) {
  case CurrentoptionMethodIdx: /* fall through */
  case CurrentoptionProcIdx:
    cscPtr = CallStackGetTopFrame0(interp);
    if (cscPtr != NULL) {
      const char *procName = Tcl_GetCommandName(interp, cscPtr->cmdPtr);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(procName, -1));
    } else {
      return NsfPrintError(interp,  "can't find proc");
    }
    break;

  case CurrentoptionMethodpathIdx:
    cscPtr = CallStackGetTopFrame0(interp);
    Tcl_SetObjResult(interp, NsfMethodNamePath(interp,
                                               CallStackGetTclFrame(interp, NULL, 1),
                                               Tcl_GetCommandName(interp, cscPtr->cmdPtr)));
    break;

  case CurrentoptionClassIdx: /* class subcommand */
    cscPtr = CallStackGetTopFrame0(interp);
    Tcl_SetObjResult(interp, (cscPtr != NULL && cscPtr->cl) ?
                     cscPtr->cl->object.cmdName : NsfGlobalObjs[NSF_EMPTY]);
    break;

  case CurrentoptionActivelevelIdx:
    Tcl_SetObjResult(interp, ComputeLevelObj(interp, ACTIVE_LEVEL));
    break;

  case CurrentoptionArgsIdx: {
    cscPtr = CallStackGetTopFrame(interp, &framePtr);

    if (cscPtr != NULL) {
      int nobjc;
      Tcl_Obj **nobjv;

      if (cscPtr->objv != NULL) {
        nobjc = cscPtr->objc;
        nobjv = (Tcl_Obj **)cscPtr->objv;
      } else {
        nobjc = Tcl_CallFrame_objc(framePtr);
        nobjv = (Tcl_Obj **)Tcl_CallFrame_objv(framePtr);
      }
      Tcl_SetObjResult(interp, Tcl_NewListObj(nobjc-1, nobjv+1));
    } else {
      return NsfPrintError(interp,  "can't find proc");
    }
    break;
  }

  case CurrentoptionActivemixinIdx: {
    NsfObject *object = NULL;
    if (RUNTIME_STATE(interp)->currentMixinCmdPtr) {
      object = NsfGetObjectFromCmdPtr(RUNTIME_STATE(interp)->currentMixinCmdPtr);
    }
    Tcl_SetObjResult(interp, (object != NULL) ? object->cmdName : NsfGlobalObjs[NSF_EMPTY]);
    break;
  }

  case CurrentoptionCalledprocIdx:
  case CurrentoptionCalledmethodIdx:
    cscPtr = CallStackFindActiveFilter(interp);
    if (cscPtr != NULL) {
      Tcl_SetObjResult(interp,
                       Tcl_NewStringObj(MethodName(cscPtr->filterStackEntry->calledProc), -1));
    } else {
      result = NsfPrintError(interp, "called from outside of a filter");
    }
    break;

  case CurrentoptionCalledclassIdx: {
    NsfClass *cl = FindCalledClass(interp, object);
    Tcl_SetObjResult(interp, (cl != NULL) ? cl->object.cmdName : NsfGlobalObjs[NSF_EMPTY]);
    break;
  }
  case CurrentoptionCallingmethodIdx:
  case CurrentoptionCallingprocIdx: {
    Tcl_Obj *resultObj;

    cscPtr = NsfCallStackFindLastInvocation(interp, 1, &framePtr);
    if ((cscPtr != NULL) && (cscPtr->cmdPtr != NULL)) {
      resultObj = NsfMethodNamePath(interp,
                                    CallStackGetTclFrame(interp, framePtr, 1),
                                    Tcl_GetCommandName(interp, cscPtr->cmdPtr));
    } else {
      resultObj = NsfGlobalObjs[NSF_EMPTY];
    }
    Tcl_SetObjResult(interp, resultObj);
    break;
  }
  case CurrentoptionCallingclassIdx:
    cscPtr = NsfCallStackFindLastInvocation(interp, 1, NULL);
    Tcl_SetObjResult(interp, (cscPtr != NULL && cscPtr->cl != NULL)
                     ? cscPtr->cl->object.cmdName
                     : NsfGlobalObjs[NSF_EMPTY]);
    break;

  case CurrentoptionCallinglevelIdx:
    if (object == NULL) {
      Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
    } else {
      Tcl_SetObjResult(interp, ComputeLevelObj(interp, CALLING_LEVEL));
    }
    break;

  case CurrentoptionCallingobjectIdx:
    cscPtr = NsfCallStackFindLastInvocation(interp, 1, NULL);
    Tcl_SetObjResult(interp, (cscPtr != NULL) ? cscPtr->self->cmdName : NsfGlobalObjs[NSF_EMPTY]);
    break;

  case CurrentoptionFilterregIdx:
    cscPtr = CallStackFindActiveFilter(interp);
    if (cscPtr != NULL) {
      Tcl_SetObjResult(interp, FilterFindReg(interp, object, cscPtr->cmdPtr));
    } else {
      result = NsfPrintError(interp, "called from outside of a filter");
    }
    break;

  case CurrentoptionIsnextcallIdx: {
    (void)CallStackGetTopFrame(interp, &framePtr);
    framePtr = CallStackNextFrameOfType(Tcl_CallFrame_callerPtr(framePtr),
                                        FRAME_IS_NSF_METHOD|FRAME_IS_NSF_CMETHOD);
    cscPtr = (framePtr != NULL) ? Tcl_CallFrame_clientData(framePtr) : NULL;

    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
                      (cscPtr != NULL && ((cscPtr->flags & NSF_CSC_CALL_IS_NEXT) != 0u)));
    break;
  }

  case CurrentoptionNextmethodIdx:
    result = FindSelfNext(interp);
    break;
  }

  return result;
}

/*
cmd self NsfSelfCmd {
}
*/
static int
NsfSelfCmd(Tcl_Interp *interp) {
  NsfObject *object = GetSelfObj(interp);

  nonnull_assert(interp != NULL);

  if (likely(object != NULL)) {
    Tcl_SetObjResult(interp, object->cmdName);
    return TCL_OK;
  } else {
    return NsfNoCurrentObjectError(interp, NULL);
  }
}

/*
cmd var::exists NsfVarExistsCmd {
  {-argName "-array" -required 0 -nrargs 0}
  {-argName "object" -required 1 -type object}
  {-argName "varName" -required 1}
}
*/
static int
NsfVarExistsCmd(Tcl_Interp *interp, int withArray, NsfObject *object, const char *varName) {
  unsigned int flags =
    NSF_VAR_TRIGGER_TRACE|NSF_VAR_REQUIRE_DEFINED|
    ((withArray != 0) ? NSF_VAR_ISARRAY : 0);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(varName != NULL);

  if (unlikely(CheckVarName(interp, varName) != TCL_OK)) {
    return TCL_ERROR;
  }
  Tcl_SetIntObj(Tcl_GetObjResult(interp), VarExists(interp, object, varName, NULL, flags));

  return TCL_OK;
}

/*
cmd var::get NsfVarGetCmd {
  {-argName "-array" -required 0 -nrargs 0}
  {-argName "object" -required 1 -type object}
  {-argName "varName" -required 1 -type tclobj}
}
*/
static int
NsfVarGetCmd(Tcl_Interp *interp, int withArray,
             NsfObject *object, Tcl_Obj *varName) {

  return NsfVarSetCmd(interp, withArray, object, varName, NULL);
}

/*
cmd var::import NsfVarImportCmd {
  {-argName "object" -type object}
  {-argName "args" -type args}
}
*/
static int NsfVarImport(Tcl_Interp *interp, NsfObject *object, const char *cmdName, int objc, Tcl_Obj *CONST objv[]) nonnull(1) nonnull(2) nonnull(3) nonnull(5);

static int
NsfVarImport(Tcl_Interp *interp, NsfObject *object, const char *cmdName, int objc, Tcl_Obj *CONST objv[]) {
  int i, result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(cmdName != NULL);
  nonnull_assert(objv != NULL);

  for (i = 0; i < objc && result == TCL_OK; i++) {
    Tcl_Obj **ov;
    int oc;

    /*fprintf(stderr, "ListGetElements %p %s\n", objv[i], ObjStr(objv[i]));*/
    if ((result = Tcl_ListObjGetElements(interp, objv[i], &oc, &ov)) == TCL_OK) {
      Tcl_Obj *varName = NULL, *alias = NULL;
      switch (oc) {
      case 0: {varName = objv[i]; break;}
      case 1: {varName = ov[0];   break;}
      case 2: {varName = ov[0];   alias = ov[1]; break;}
      }
      if (likely(varName != NULL)) {
        result = ImportInstVarIntoCurrentScope(interp, cmdName, object, varName, alias);
      } else {
        assert(objv[i]);
        result = NsfPrintError(interp, "invalid variable specification '%s'", ObjStr(objv[i]));
      }
    }
  }

  return result;
}

static int
NsfVarImportCmd(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[]) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return NsfVarImport(interp, object, "importvar", objc, objv);
}

/*
cmd var::set NsfVarSetCmd {
  {-argName "-array" -required 0 -nrargs 0}
  {-argName "object" -required 1 -type object}
  {-argName "varName" -required 1 -type tclobj}
  {-argName "value" -required 0 -type tclobj}
}
*/
static int
NsfVarSetCmd(Tcl_Interp *interp, int withArray,
             NsfObject *object, Tcl_Obj *varName, Tcl_Obj *valueObj) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(varName != NULL);

  if (unlikely(CheckVarName(interp, ObjStr(varName)) != TCL_OK)) {
    return TCL_ERROR;
  }
  if (withArray != 0) {
    return SetInstArray(interp, object, varName, valueObj);
  } else {
    return SetInstVar(interp, object, varName, valueObj);
  }
}

/*
cmd var::unset NsfVarUnsetCmd {
  {-argName "-nocomplain" -required 0 -nrargs 0}
  {-argName "object" -required 1 -type object}
  {-argName "varName" -required 1 -type tclobj}
}
*/
static int
NsfVarUnsetCmd(Tcl_Interp *interp, int withNocomplain, NsfObject *object, Tcl_Obj *varNameObj) {
  const char *varName = ObjStr(varNameObj);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(varNameObj != NULL);

  if (unlikely(CheckVarName(interp, varName) != TCL_OK)) {
    return TCL_ERROR;
  }

  return UnsetInstVar(interp, withNocomplain, object, varName);
}
/***********************************************************************
 * End generated Next Scripting  commands
 ***********************************************************************/

/*
 * Parameter support functions
 */

typedef struct NsfParamWrapper {
  Nsf_Param *paramPtr;
  int refCount;
  int canFree;
} NsfParamWrapper;

static Tcl_DupInternalRepProc      ParamDupInteralRep;
static Tcl_FreeInternalRepProc     ParamFreeInternalRep;
static Tcl_UpdateStringProc        ParamUpdateString;

static void ParamUpdateString(Tcl_Obj *objPtr) nonnull(1);
static void ParamDupInteralRep(Tcl_Obj *srcPtr, Tcl_Obj *UNUSED(dupPtr)) nonnull(1);
static void ParamFreeInternalRep(register Tcl_Obj *objPtr) nonnull(1);
static int ParamSetFromAny(Tcl_Interp *interp,        register Tcl_Obj *objPtr) nonnull(1) nonnull(2);
static int ParamSetFromAny2(Tcl_Interp *interp, const char *varNamePrefix,
                            int configureParameter, register Tcl_Obj *objPtr)
  nonnull(1) nonnull(2) nonnull(4);

static void
ParamUpdateString(Tcl_Obj *objPtr) {
  nonnull_assert(objPtr != NULL);
  Tcl_Panic("%s of type %s should not be called", "updateStringProc",
            objPtr->typePtr->name);
}

static void
ParamDupInteralRep(Tcl_Obj *srcPtr, Tcl_Obj *UNUSED(dupPtr)) {
  nonnull_assert(srcPtr != NULL);
  Tcl_Panic("%s of type %s should not be called", "dupStringProc",
            srcPtr->typePtr->name);
}

static Tcl_ObjType paramObjType = {
    "nsfParam",                                /* name */
    ParamFreeInternalRep,                /* freeIntRepProc */
    ParamDupInteralRep,                        /* dupIntRepProc */
    ParamUpdateString,                        /* updateStringProc */
    ParamSetFromAny                        /* setFromAnyProc */
};

static void
ParamFreeInternalRep(
    register Tcl_Obj *objPtr)        /* Param structure object with internal
                                 * representation to free. */
{
  NsfParamWrapper *paramWrapperPtr = (NsfParamWrapper *)objPtr->internalRep.twoPtrValue.ptr1;

  nonnull_assert(objPtr != NULL);
  if (paramWrapperPtr != NULL) {
    /* fprintf(stderr, "ParamFreeInternalRep freeing wrapper %p paramPtr %p refCount %dcanFree %d\n",
            paramWrapperPtr, paramWrapperPtr->paramPtr, paramWrapperPtr->refCount,
            paramWrapperPtr->canFree);*/

    if (paramWrapperPtr->canFree == 1) {
      ParamsFree(paramWrapperPtr->paramPtr);
      FREE(NsfParamWrapper, paramWrapperPtr);
    } else {
      paramWrapperPtr->refCount--;
    }
  }
}

/*
 *----------------------------------------------------------------------
 * ParamSetFromAny2 --
 *
 *    Convert the second argument argument (e.g. "x:integer") into the
 *    internal representation of a Tcl_Obj of the type parameter. The
 *    conversion is performed by the usual ParamParse() function, used
 *    e.g. for the parameter passing for arguments.
 *
 * Results:
 *    Result code.
 *
 * Side effects:
 *    Converted internal rep of Tcl_Obj
 *
 *----------------------------------------------------------------------
 */

static int
ParamSetFromAny2(
    Tcl_Interp *interp,               /* Used for error reporting if not NULL. */
    const char *varNamePrefix,        /* shows up as varName in error message */
    int allowParameter,               /* allow object parameters */
    register Tcl_Obj *objPtr)         /* The object to convert. */
{
  Tcl_Obj *fullParamObj = Tcl_NewStringObj(varNamePrefix, -1);
  int result, possibleUnknowns = 0, plainParams = 0, nrNonposArgs = 0;
  NsfParamWrapper *paramWrapperPtr = NEW(NsfParamWrapper);

  nonnull_assert(interp != NULL);
  nonnull_assert(varNamePrefix != NULL);
  nonnull_assert(objPtr != NULL);

  paramWrapperPtr->paramPtr = ParamsNew(1);
  paramWrapperPtr->refCount = 1;
  paramWrapperPtr->canFree = 0;

  Tcl_AppendLimitedToObj(fullParamObj, ObjStr(objPtr), -1, INT_MAX, NULL);
  INCR_REF_COUNT(fullParamObj);
  result = ParamParse(interp, NsfGlobalObjs[NSF_VALUECHECK], fullParamObj,
                      (allowParameter == 1) ? NSF_DISALLOWED_ARG_OBJECT_PARAMETER : NSF_DISALLOWED_ARG_VALUECHECK,
                      paramWrapperPtr->paramPtr, &possibleUnknowns,
                      &plainParams, &nrNonposArgs);
  /*
   * We treat currently unknown user level converters as error.
   */
  if (unlikely((paramWrapperPtr->paramPtr->flags & NSF_ARG_CURRENTLY_UNKNOWN) != 0u)) {
    result = TCL_ERROR;
  }

  if (likely(result == TCL_OK)) {
    /*
     * In success cases, the memory allocated by this function is freed via
     * the tcl_obj type.
     */
    paramWrapperPtr->paramPtr->flags |= NSF_ARG_UNNAMED;
    if (*(paramWrapperPtr->paramPtr->name) == 'r') {
      paramWrapperPtr->paramPtr->flags |= NSF_ARG_IS_RETURNVALUE;
    }
    TclFreeIntRep(objPtr);
    objPtr->internalRep.twoPtrValue.ptr1 = (void *)paramWrapperPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &paramObjType;
  } else {
    /*
     * In error cases, free manually memory allocated by this function.
     */
    ParamsFree(paramWrapperPtr->paramPtr);
    FREE(NsfParamWrapper, paramWrapperPtr);
  }

  DECR_REF_COUNT(fullParamObj);
  return result;
}

static int
ParamSetFromAny(
    Tcl_Interp *interp,                /* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr)         /* The object to convert. */
{
  nonnull_assert(interp != NULL);
  nonnull_assert(objPtr != NULL);

  return ParamSetFromAny2(interp, "value:", 0, objPtr);
}

/*
 *----------------------------------------------------------------------
 * GetObjectParameterDefinition --
 *
 *    Obtain the parameter definitions for an object by calling the method
 *    "__objectparameter" if the value is not cached already. Caching is
 *    performed on the class, the cached values are used in case there are no
 *    object-specific slots.
 *
 * Results:
 *    Tcl return code, parsed structure in last argument
 *
 * Side effects:
 *    Updates potentially cl->parsedParamPtr
 *
 *----------------------------------------------------------------------
 */
static int
ComputeParameterDefinition(Tcl_Interp *interp, Tcl_Obj *procNameObj,
                           NsfObject *object, NsfClass *class,
                           NsfParsedParam *parsedParamPtr) {
  int result = TCL_OK;
  Tcl_Obj *methodObj;
  NsfObject *self;

  if (object != NULL) {
    methodObj = NsfMethodObj(object, NSF_o_configureparameter_idx);
    self = object;
  } else {
    assert(class != NULL);
    self = &class->object;
    methodObj = NsfMethodObj(self, NSF_c_configureparameter_idx);
  }

  if (methodObj != NULL) {
    /*fprintf(stderr, "calling %s %s\n", ObjectName(self), ObjStr(methodObj));*/
    result = CallMethod(self, interp, methodObj, 2, NULL,
                        NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE);

    if (likely(result == TCL_OK)) {
      Tcl_Obj *rawConfArgs = Tcl_GetObjResult(interp);

      /*fprintf(stderr, ".... rawConfArgs for %s => '%s'\n",
        ObjectName(self), ObjStr(rawConfArgs));*/
      INCR_REF_COUNT(rawConfArgs);

      /*
       * Parse the string representation to obtain the internal
       * representation.
       */
      result = ParamDefsParse(interp, procNameObj, rawConfArgs,
                              NSF_DISALLOWED_ARG_OBJECT_PARAMETER, 1,
                              parsedParamPtr);
      if (likely(result == TCL_OK)) {
        NsfParsedParam *ppDefPtr = NEW(NsfParsedParam);

        ppDefPtr->paramDefs = parsedParamPtr->paramDefs;
        ppDefPtr->possibleUnknowns = parsedParamPtr->possibleUnknowns;
        if (class != NULL) {
          assert(class->parsedParamPtr == NULL);
          class->parsedParamPtr = ppDefPtr;
#if defined(PER_OBJECT_PARAMETER_CACHING)
        } else if (object != NULL) {
          NsfObjectOpt *opt = NsfRequireObjectOpt(object);
          if (object->opt->parsedParamPtr != NULL) {
            NsfParameterCacheObjectInvalidateCmd(interp, object);
          }
          opt->parsedParamPtr = ppDefPtr;
          opt->classParamPtrEpoch = RUNTIME_STATE(interp)->classParamPtrEpoch;
          /*fprintf(stderr, "set obj param for obj %p %s epoch %d ppDefPtr %p\n",
            object, ObjectName(object), opt->classParamPtrEpoch, ppDefPtr);*/
#endif
        }
        if (ppDefPtr->paramDefs != NULL) {
          ParamDefsRefCountIncr(ppDefPtr->paramDefs);
        }
      }
      DECR_REF_COUNT(rawConfArgs);
    }
  }
  return result;
}


static int
GetObjectParameterDefinition(Tcl_Interp *interp, Tcl_Obj *procNameObj,
                             NsfObject *object, NsfClass *class,
                             NsfParsedParam *parsedParamPtr) {
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(procNameObj != NULL);
  nonnull_assert(parsedParamPtr != NULL);

  parsedParamPtr->paramDefs = NULL;
  parsedParamPtr->possibleUnknowns = 0;

  if (class == NULL) {
    assert(object != NULL);
    if ((object->flags & NSF_HAS_PER_OBJECT_SLOTS) != 0u
        || (object->opt != NULL && object->opt->objMixins)
        ) {
      /*
       * We have object-specific parameters.  Do not use the per-class cache,
       * and do not save the results in the per-class cache
       */
      /*fprintf(stderr, "per-object configure obj %s flags %.6x\n",
        ObjectName(object), object->flags);*/
    } else {
      class = object->cl;
    }
  }

  /*
   * Parameter definitions are cached in the class, for which
   * instances are created. The parameter definitions are flushed in
   * the following situations:
   *
   * a) on class cleanup: ParsedParamFree(cl->parsedParamPtr)
   * b) on class structure changes,
   * c) when class-mixins are added,
   * d) when new slots are defined,
   * e) when slots are removed
   *
   * When slot defaults or types are changed, the slots have to
   * perform a manual "::nsf::invalidateobjectparameter $domain".
   */

  /*
   * Check, if there is already a parameter definition available for
   * creating objects of this class.
   */

  if (likely(class != NULL && class->parsedParamPtr != NULL)) {
    NsfParsedParam *clParsedParamPtr = class->parsedParamPtr;

    parsedParamPtr->paramDefs = clParsedParamPtr->paramDefs;
    parsedParamPtr->possibleUnknowns = clParsedParamPtr->possibleUnknowns;
    result = TCL_OK;

#if defined(PER_OBJECT_PARAMETER_CACHING)
  } else if (object != NULL && object->opt != NULL && object->opt->parsedParamPtr != NULL &&
             object->opt->classParamPtrEpoch == RUNTIME_STATE(interp)->classParamPtrEpoch) {
    NsfParsedParam *objParsedParamPtr = object->opt->parsedParamPtr;

    /*fprintf(stderr, "reuse obj param for obj %p  %s paramPtr %p\n",
      object, ObjectName(object), objParsedParamPtr);*/
    parsedParamPtr->paramDefs = objParsedParamPtr->paramDefs;
    parsedParamPtr->possibleUnknowns = objParsedParamPtr->possibleUnknowns;
    result = TCL_OK;
#endif

  } else {
    /*
     * There is no parameter definition available, get a new one in
     * the the string representation.
     */
    result = ComputeParameterDefinition(interp, procNameObj,
                                        object, class,
                                        parsedParamPtr);
  }

  return result;
}

/*
 *----------------------------------------------------------------------
 * ParameterCheck --
 *
 *    Check the provided valueObj against the parameter specification provided
 *    in the second argument (paramObjPtr), when doCheckArguments is true. This
 *    function is used e.g. by nsf::is, where only the right hand side of a
 *    parameter specification (after the colon) is specified. The argument
 *    Name (before the colon in a parameter spec) is provided via
 *    argNamePrefix. The converted parameter structure is returned optionally
 *    via the last argument.
 *
 * Results:
 *    Tcl return code, parsed structure in last argument
 *
 * Side effects:
 *    Converts potentially tcl_obj type of paramObjPtr
 *
 *----------------------------------------------------------------------
 */

static int
ParameterCheck(Tcl_Interp *interp, Tcl_Obj *paramObjPtr, Tcl_Obj *valueObj,
               const char *argNamePrefix, int doCheckArguments,
               int isNamed, int doConfigureParameter,
               Nsf_Param **paramPtrPtr) {
  Nsf_Param *paramPtr;
  NsfParamWrapper *paramWrapperPtr;
  Tcl_Obj *outObjPtr = NULL;
  ClientData checkedData;
  int result;
  unsigned int flags = 0u;

  nonnull_assert(interp != NULL);
  nonnull_assert(paramObjPtr != NULL);
  nonnull_assert(valueObj != NULL);

  /*fprintf(stderr, "ParameterCheck %s value %p %s\n",
    ObjStr(paramObjPtr), valueObj, ObjStr(valueObj));*/

  if (paramObjPtr->typePtr == &paramObjType) {
    paramWrapperPtr = (NsfParamWrapper *) paramObjPtr->internalRep.twoPtrValue.ptr1;
  } else {
    /*
     * We could use in principle Tcl_ConvertToType(..., &paramObjType) instead
     * of checking the type manually, but we want to pass the argNamePrefix
     * explicitly.
     */
    result = ParamSetFromAny2(interp, argNamePrefix, doConfigureParameter, paramObjPtr);
    if (likely(result == TCL_OK)) {
      paramWrapperPtr = (NsfParamWrapper *) paramObjPtr->internalRep.twoPtrValue.ptr1;
    } else {
      return NsfPrintError(interp, "invalid value constraints \"%s\"", ObjStr(paramObjPtr));
    }
  }
  paramPtr = paramWrapperPtr->paramPtr;
  if (paramPtrPtr != NULL) *paramPtrPtr = paramPtr;

  if (isNamed != 0) {
    paramPtr->flags &= ~NSF_ARG_UNNAMED;
  }

  result = ArgumentCheck(interp, valueObj, paramPtr, doCheckArguments, &flags, &checkedData, &outObjPtr);
  /*fprintf(stderr, "ParameterCheck paramPtr %p final refCount of wrapper %d can free %d flags %.6x\n",
    paramPtr, paramWrapperPtr->refCount,  paramWrapperPtr->canFree, flags);*/

  assert(paramWrapperPtr->refCount > 0);
  paramWrapperPtr->canFree = 1;

  if ((flags & NSF_PC_MUST_DECR) != 0u) {
    DECR_REF_COUNT2("valueObj", outObjPtr);
  }

  return result;
}



/***********************************************************************
 * Begin Object Methods
 ***********************************************************************/
/*
objectMethod autoname NsfOAutonameMethod {
  {-argName "-instance"}
  {-argName "-reset"}
  {-argName "name" -required 1 -type tclobj}
}
*/
static int
NsfOAutonameMethod(Tcl_Interp *interp, NsfObject *object, int withInstance, int withReset,
                   Tcl_Obj *nameObj) {
  Tcl_Obj *autonamedObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(nameObj != NULL);

  autonamedObj = AutonameIncr(interp, nameObj, object, withInstance, withReset);
  if (autonamedObj != NULL) {
    Tcl_SetObjResult(interp, autonamedObj);
    DECR_REF_COUNT2("autoname", autonamedObj);
    return TCL_OK;
  }

  return NsfPrintError(interp, "autoname failed. Probably format string (with %%) was not well-formed");
}

/*
objectMethod class NsfOClassMethod {
  {-argName "class" -required 0 -type tclobj}
}
*/
static int
NsfOClassMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *classObj) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return NsfRelationSetCmd(interp, object, RelationtypeClassIdx, classObj);
}

/*
objectMethod cleanup NsfOCleanupMethod {
}
*/
static int
NsfOCleanupMethod(Tcl_Interp *interp, NsfObject *object) {
  NsfClass *cl = NsfObjectToClass(object);
  Tcl_Obj *savedNameObj;
  int softrecreate;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#if defined(OBJDELETION_TRACE)
  fprintf(stderr, "+++ NsfOCleanupMethod\n");
#endif
  PRINTOBJ("NsfOCleanupMethod", object);

  savedNameObj = object->cmdName;
  INCR_REF_COUNT(savedNameObj);

  /* save and pass around softrecreate*/
  softrecreate =
    (object->flags & NSF_RECREATE) != 0u
    && RUNTIME_STATE(interp)->doSoftrecreate;

  CleanupDestroyObject(interp, object, softrecreate);
  CleanupInitObject(interp, object, object->cl, object->nsPtr, softrecreate);

  if (cl != NULL) {
    CleanupDestroyClass(interp, cl, softrecreate, 1);
    CleanupInitClass(interp, cl, cl->nsPtr, softrecreate, 1);
  }

  DECR_REF_COUNT(savedNameObj);
  return TCL_OK;
}

/*
objectMethod configure NsfOConfigureMethod {
  {-argName "args" -type allargs}
}
*/

static NsfObject* GetSlotObject(Tcl_Interp *interp, Tcl_Obj *slotObj) nonnull(1) nonnull(2);

static NsfObject*
GetSlotObject(Tcl_Interp *interp, Tcl_Obj *slotObj) {
  NsfObject *slotObject = NULL;

  nonnull_assert(interp != NULL);
  nonnull_assert(slotObj != NULL);

  if (unlikely(GetObjectFromObj(interp, slotObj, &slotObject) != TCL_OK || slotObject == NULL)) {
    NsfPrintError(interp, "couldn't resolve slot object %s", ObjStr(slotObj));
  }

  return slotObject;
}


static int
NsfOConfigureMethod(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[], Tcl_Obj *objv0) {
  int result, i;
  NsfParsedParam parsedParam;
  Nsf_Param *paramPtr;
  NsfParamDefs *paramDefs;
  Tcl_Obj *newValue, *initMethodObj;
  const char *initString;
  ParseContext pc;
  CallFrame frame, *framePtr = &frame, *uplevelVarFramePtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(objv != NULL);

#if 0
  fprintf(stderr, "NsfOConfigureMethod %s.%s flags %.6x oc %2d", ObjectName(object), ObjStr(objv0), object->flags, objc);
  for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
  fprintf(stderr, "\n");
#endif

  /* Get the object parameter definition */
  assert(objv0 != NULL);
  result = GetObjectParameterDefinition(interp, objv0, object, NULL, &parsedParam);

  if (result != TCL_OK || parsedParam.paramDefs == NULL) {
    /*fprintf(stderr, "... nothing to do for method %s\n", ObjStr(objv0));*/
    return result;
  }

  /*
   * Get the initMethodObj/initString outside the loop iterating over the
   * arguments.
   */
  if (CallDirectly(interp, object, NSF_o_init_idx, &initMethodObj)) {
    initString = NULL;
  } else {
    initString = ObjStr(initMethodObj);
  }

  /*
   * The effective call site of the configure() method (e.g., a proc or a
   * method) can result from up-leveling the object creation procedure;
   * therefore, the *effective* call site can deviate from the *declaring*
   * call site (e.g. as in XOTcl2's unknown method). In such a scenario, the
   * configure() dispatch finds itself in a particular call-stack
   * configuration: The top-most frame reflects the declaring call site
   * (interp->framePtr), while the effective call site (interp->varFramePtr)
   * is identified by a lower call-stack level.
   *
   * Since configure pushes an object frame (for accessing the instance
   * variables) and sometimes a CMETHOD frame (for method invocations) we
   * record a) whether there was a preceding uplevel (identifiable through
   * deviating interp->framePtr and interp->varFramePtr) and, in case, b) the
   * ruling variable frame context. The preserved call-frame reference can
   * later be used to restore the uplevel'ed call frame context.
   */

  uplevelVarFramePtr =
    (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp) != Tcl_Interp_framePtr(interp)
    ? Tcl_Interp_varFramePtr(interp)
    : NULL;

  /*
   * Push frame to allow for [self] and make instance variables of obj
   * accessible as locals.
   */
  Nsf_PushFrameObj(interp, object, framePtr);

  /* Process the actual arguments based on the parameter definitions */
  paramDefs = parsedParam.paramDefs;
  ParamDefsRefCountIncr(paramDefs);
  result = ProcessMethodArguments(&pc, interp, object,
                                  NSF_ARGPARSE_START_ZERO, paramDefs,
                                  NsfGlobalObjs[NSF_CONFIGURE], objc, objv);

  if (unlikely(result != TCL_OK)) {
    Nsf_PopFrameObj(interp, framePtr);
    goto configure_exit;
  }

  /*
   * At this point, the arguments are tested to be valid (according to the
   * parameter definitions) and the defaults are set. Now we have to apply the
   * arguments (mostly setting instance variables).
   */

#if defined(CONFIGURE_ARGS_TRACE)
  fprintf(stderr, "*** POPULATE OBJ '%s': nr of parsed args %d\n", ObjectName(object), pc.objc);
#endif
  for (i = 1, paramPtr = paramDefs->paramsPtr; paramPtr->name; paramPtr++, i++) {

    /*
     * Set the new value always when the new value was specified (was not
     * taken from the default). When we take the default, we do not overwrite
     * already existing values (which might have been set via parameter
     * alias).
     */
    /*fprintf(stderr, "[%d] param %s, object init called %d is default %d value = '%s' nrArgs %d\n",
            i, paramPtr->name, (object->flags & NSF_INIT_CALLED),
            (pc.flags[i-1] & NSF_PC_IS_DEFAULT),
            ObjStr(pc.full_objv[i]), paramPtr->nrArgs);*/

    if ((pc.flags[i-1] & NSF_PC_IS_DEFAULT)) {
      /*
       * Object parameter method calls (when the flag
       * NSF_ARG_METHOD_INVOCATION is set) do not set instance variables, so
       * we do not have to check for existing variables.
       */
      if ((paramPtr->flags & NSF_ARG_METHOD_INVOCATION) == 0u) {
        Tcl_Obj *varObj = Tcl_ObjGetVar2(interp, paramPtr->nameObj, NULL, TCL_PARSE_PART1);

        if (varObj != NULL) {
          /*
           * The value exists already, ignore this parameter.
           */
          /*fprintf(stderr, "a variable for %s exists already, "
                  "ignore param flags %.6x valueObj %p\n",
                  paramPtr->name, paramPtr->flags, pc.full_objv[i]);*/
          continue;
        }
      } else if ((object->flags & NSF_INIT_CALLED) != 0u) {
        /*
         * The object is already initialized. Don't use the default, since it
         * might change part of the state back to the original default.  This
         * might happen, when e.g. configure is called on a class manually,
         * where "superclass" has a default.
         */
        /*fprintf(stderr, "%s skip default %s in configure\n",
          ObjectName(object), ObjStr(pc.full_objv[i]));*/
        continue;
      }
    } else if (unlikely((paramPtr->flags & NSF_ARG_REQUIRED) != 0u
                        && pc.full_objv[i] == NsfGlobalObjs[NSF___UNKNOWN__])) {

      /* Previous versions contained a test for
       *   (object->flags & NSF_INIT_CALLED)
       *
       * to perform required testing just for in the non-initialized state. We
       * switched in 2.0b5 to checking for the existance of the associated
       * instance variable, which works under the assumption that the instance
       * variable has the same name and that e.g. an required alias parameter
       * sets this variable either. Similar assumption is in the default
       * handling. Future versions might use a more general handling of the
       * parameter states.
       */

      Tcl_Obj *varObj = Tcl_ObjGetVar2(interp, paramPtr->nameObj, NULL, TCL_PARSE_PART1);
      if (unlikely(varObj == NULL)) {
        Tcl_Obj *paramDefsObj = NsfParamDefsSyntax(interp, paramDefs->paramsPtr, object, NULL);

        NsfPrintError(interp, "required argument '%s' is missing, should be:\n\t%s%s%s %s", (paramPtr->nameObj != NULL) ? ObjStr(paramPtr->nameObj) : paramPtr->name, (pc.object != NULL) ? ObjectName(pc.object) : "", (pc.object != NULL) ? " " : "",
                      ObjStr(pc.full_objv[0]),
                      ObjStr(paramDefsObj));
        DECR_REF_COUNT2("paramDefsObj", paramDefsObj);

        Nsf_PopFrameObj(interp, framePtr);
        result = TCL_ERROR;
        goto configure_exit;
      }
    }

    newValue = pc.full_objv[i];
    /*fprintf(stderr, "     new Value of %s = [%d] %p '%s', type %s addr %p\n",
            ObjStr(paramPtr->nameObj), i,
            newValue, (newValue != NULL) ? ObjStr(newValue) : "(null)", paramPtr->type,
            &(pc.full_objv[i]));*/

    /*
     * Handling slot initialize
     */
    if ((paramPtr->flags & NSF_ARG_SLOTINITIALIZE) != 0u) {
      NsfObject *slotObject = GetSlotObject(interp, paramPtr->slotObj);

      if (likely(slotObject != NULL)) {
        Tcl_Obj *ov[1];

        ov[0] = paramPtr->nameObj;
        result = NsfCallMethodWithArgs(interp, (Nsf_Object *)slotObject, NsfGlobalObjs[NSF_INITIALIZE],
                                       object->cmdName, 2, ov,
                                       NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS);

      }
      if (unlikely(result != TCL_OK)) {
        /*
         * The error message was set either by GetSlotObject or by ...CallMethod...
         */
        Nsf_PopFrameObj(interp, framePtr);
        goto configure_exit;
      }
    }

    /*
     * Special setter methods for invoking methods calls; handles types
     * "cmd", "initcmd", "alias" and "forward".
     */
    if ((paramPtr->flags & NSF_ARG_METHOD_INVOCATION) != 0u) {
      int consuming = (*paramPtr->name == '-' || paramPtr->nrArgs > 0);

      if (consuming && newValue == NsfGlobalObjs[NSF___UNKNOWN__]) {
        /*
         * In the case we have a consuming parameter, but we have no value
         * provided and not default, there is no reason to call the invocation
         * parameter.
         */
        /*fprintf(stderr, "%s consuming nrargs %d no value\n", paramPtr->name, paramPtr->nrArgs);*/
        continue;
      }

      if ((paramPtr->flags & NSF_ARG_INITCMD) != 0u) {

        if (paramPtr->defaultValue != NULL) {
          /*
           * The "defaultValue" holds the initcmd to be executed
           */
          Tcl_Obj *varObj = Tcl_ObjGetVar2(interp, NsfGlobalObjs[NSF_ARRAY_INITCMD],
                                           paramPtr->nameObj, 0);
          if (varObj == NULL) {
            /*
             * The variable is not set. Therefore, we assume, we have to
             * execute the initcmd. On success, we note the execution in the NSF_ARRAY_INITCMD
             * variable (usually __initcmd(name))
             */
            result = ParameterMethodDispatch(interp, object, paramPtr, paramPtr->defaultValue,
                                             uplevelVarFramePtr, initString,
                                             (Tcl_Obj **)&objv[pc.lastObjc],
                                             objc - pc.lastObjc);

            if (unlikely(result != TCL_OK)) {
              Nsf_PopFrameObj(interp, framePtr);
              goto configure_exit;
            }
            if (unlikely(Tcl_ObjSetVar2(interp, NsfGlobalObjs[NSF_ARRAY_INITCMD],
                                        paramPtr->nameObj, Tcl_NewIntObj(1), TCL_LEAVE_ERR_MSG) == NULL)) {
              Nsf_PopFrameObj(interp, framePtr);
              goto configure_exit;
            }
          }

        } else {
          /*
           * We could consider to require a default
           */
        }
        /*
         * if we have a new actual value, proceed to setvars
         */
        if ((pc.flags[i-1] & NSF_PC_IS_DEFAULT) == 0) {
          goto setvars;
        }
        continue;
      }
      /*
       * lastObjc points to the first "unprocessed" argument, the argument before should be valid, when lastObjc > 1
       */
      if (pc.lastObjc > 1) {
        assert(ISOBJ(objv[pc.lastObjc-1]));
      }
      result = ParameterMethodDispatch(interp, object, paramPtr, newValue,
                                       uplevelVarFramePtr, initString,
                                       (Tcl_Obj **)&objv[pc.lastObjc],
                                       objc - pc.lastObjc);
      if (unlikely(result != TCL_OK)) {
        Nsf_PopFrameObj(interp, framePtr);
        goto configure_exit;
      }
      continue;
    }

  setvars:
    if (newValue == NsfGlobalObjs[NSF___UNKNOWN__]) {
      /*
       * Nothing to do, we have a value setter, but no value is specified and
       * no default was provided.
       */
      continue;
    }

    /*
     * Set the instance variable unless the last argument of the
     * definition is varArgs.
     */
    if (i < paramDefs->nrParams || pc.varArgs == 0) {

#if defined(CONFIGURE_ARGS_TRACE)
      fprintf(stderr, "*** %s SET %s '%s' // %p\n",
              ObjectName(object), ObjStr(paramPtr->nameObj), ObjStr(newValue), paramPtr->slotObj);
#endif
      /*
       * Actually set instance variable with the provided value or default
       * value. In case, explicit invocation of the setter is needed, we call the method, which
       * is typically a forwarder to the slot object.
       */

      if ((paramPtr->flags & NSF_ARG_SLOTSET) != 0u) {
        NsfObject *slotObject = GetSlotObject(interp, paramPtr->slotObj);

        if (likely(slotObject != NULL)) {
          Tcl_Obj *ov[2];
          Tcl_Obj *methodObj = NsfMethodObj(object, NSF_s_set_idx);

          ov[0] = (paramPtr->method != NULL) ? paramPtr->method : paramPtr->nameObj;
          ov[1] = newValue;

          /*fprintf(stderr, "SLOTSET %s %s %s %s %s idx %d %p\n", ObjectName(slotObject),
                  ObjStr(NsfGlobalObjs[NSF_SET]), ObjStr(object->cmdName),
                  ObjStr(paramPtr->nameObj), ObjStr(newValue),
                  NSF_s_set_idx, methodObj);*/

          result = NsfCallMethodWithArgs(interp, (Nsf_Object *)slotObject, (methodObj != NULL) ? methodObj : NsfGlobalObjs[NSF_SLOT_SET],
                                         object->cmdName, 3, ov, NSF_CSC_IMMEDIATE);
        }
        if (unlikely(result != TCL_OK)) {
          /*
           * The error message was set either by GetSlotObject or by ...CallMethod...
           */
          Nsf_PopFrameObj(interp, framePtr);
          goto configure_exit;
        }
      } else {
        Tcl_Obj *resultObj;
        /*
         * Plain set of the variable.
         */
        resultObj = Tcl_ObjSetVar2(interp, paramPtr->nameObj, NULL, newValue, TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1);
        if (unlikely(resultObj == NULL)) {
          /*
           * When the setting of the variable failed (e.g. caused by variable
           * traces), report the error back.
           */
          result = TCL_ERROR;
          Nsf_PopFrameObj(interp, framePtr);
          goto configure_exit;
        }
      }
    }
  }

  Nsf_PopFrameObj(interp, framePtr);

 configure_exit:

  ParamDefsRefCountDecr(paramDefs);
  ParseContextRelease(&pc);

  if (likely(result == TCL_OK)) {
    Tcl_ResetResult(interp);
  }
  return result;
}

/*
objectMethod cget NsfOCgetMethod {
  {-argName "name" -type tclobj -required 1}
}
*/
static int
NsfOCgetMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *nameObj) {
  int result, found = 0;
  NsfParsedParam parsedParam;
  Nsf_Param const *paramPtr;
  NsfParamDefs *paramDefs;
  CallFrame frame, *framePtr = &frame, *uplevelVarFramePtr;
  const char *nameString = ObjStr(nameObj);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(nameObj != NULL);

  /*
   * Get the object parameter definition
   */
  result = GetObjectParameterDefinition(interp, NsfGlobalObjs[NSF_EMPTY],
                                        object, NULL, &parsedParam);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  /*
   * GetObjectParameterDefinition() was returning TCL_OK, the paramdefs have
   * to be set.
   */
  assert(parsedParam.paramDefs);

  /*
   * We do not stack a plain stack fraom NSF_CSC_TYPE_PLAIN here, as we do in
   * NsfOConfigureMethod (but maybe we have to for full compatibility TODO:
   * check and compar with configure stack setup). Therefore we pass NULL as
   * cscPtr to ParameterMethodForwardDispatch).
   */

  /*
   * The uplevel handling is exactly the same as in NsfOConfigureMethod() and
   * is needed, when methods are called, which perform an upvar.
   */
  uplevelVarFramePtr =
    (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp) != Tcl_Interp_framePtr(interp)
    ? Tcl_Interp_varFramePtr(interp)
    : NULL;

  /*
   * Push frame to allow invocations of [self] and make instance variables of
   * obj accessible as locals.
   */
  Nsf_PushFrameObj(interp, object, framePtr);

  paramDefs = parsedParam.paramDefs;
  ParamDefsRefCountIncr(paramDefs);

  /*
   * Does provided value start with a dash?
   */
  if (*nameString == '-') {
    /*
     * Skip leading parameters from the definition, which are no nonpos args
     * (very unlikely).
     */
    for (paramPtr = paramDefs->paramsPtr;
         paramPtr->name && *paramPtr->name != '-';
         paramPtr++);

    /*
     * Perform the lookup from the next group.
     */
    if (unlikely(NsfParamDefsNonposLookup(interp, nameString, paramPtr, &paramPtr) != TCL_OK)) {
      result = TCL_ERROR;
      goto cget_exit;
    } else {
      found = (paramPtr != NULL);
    }
  }

  if (found == 0) {
    result = NsfPrintError(interp, "cget: unknown configure parameter %s", nameString);
    goto cget_exit;
  }

  /*fprintf(stderr, "cget: arg %s found, flags %.8x\n", nameString, paramPtr->flags);*/

  /*
   * Check for slot invocation
   */
  if (paramPtr->slotObj != NULL) {
    NsfObject *slotObject = GetSlotObject(interp, paramPtr->slotObj);
    Tcl_Obj *methodObj = NsfMethodObj(object, NSF_s_get_idx);
    Tcl_Obj *ov[1];

    /*
     * Get instance variable via slot.
     */
    if (uplevelVarFramePtr != NULL) {
      Tcl_Interp_varFramePtr(interp) = uplevelVarFramePtr;
    }
    ov[0]  = (paramPtr->method != NULL) ? paramPtr->method : paramPtr->nameObj;

    /*fprintf(stderr, "SLOTGET %s idx %d %p\n", ObjectName(slotObject),
      NSF_s_get_idx, methodObj);*/

    result = NsfCallMethodWithArgs(interp, (Nsf_Object *)slotObject, (methodObj != NULL) ? methodObj : NsfGlobalObjs[NSF_SLOT_GET],
                                   object->cmdName, 2, ov, NSF_CSC_IMMEDIATE);
    goto cget_exit;
  }

  /*
   * We do NOT have a slot
   */
  if (found && (paramPtr->flags & NSF_ARG_METHOD_CALL) != 0u) {
    if ((paramPtr->flags & NSF_ARG_ALIAS) != 0u) {
      /*
       * It is a parameter associated with an aliased method. Invoke the
       * method without an argument.
       */
      Tcl_Obj *methodObj = (paramPtr->method != NULL) ? paramPtr->method : paramPtr->nameObj;

      if (uplevelVarFramePtr != NULL) {
        Tcl_Interp_varFramePtr(interp) = uplevelVarFramePtr;
      }

      result = CallMethod(object, interp, methodObj, 2, NULL, NSF_CSC_IMMEDIATE);
    } else {
      /*
       * Must be NSF_ARG_FORWARD
       */
      assert((paramPtr->flags & NSF_ARG_FORWARD) != 0u);

      /* since we have no cscPtr, we provide NULL */
      result = ParameterMethodForwardDispatch(interp, object,
                                              paramPtr, NULL, NULL /* cscPtr */);
    }
  } else {
    /*
     * Must be a parameter associated with a variable
     */
    unsigned int flags = (object->nsPtr != NULL) ? (TCL_LEAVE_ERR_MSG|TCL_NAMESPACE_ONLY) : TCL_LEAVE_ERR_MSG;
    Tcl_Obj *resultObj = Tcl_ObjGetVar2(interp, paramPtr->nameObj, NULL, flags);

    if (resultObj != NULL) {
      /*
       * The value exists
       */
      Tcl_SetObjResult(interp, resultObj);
    }
  }

 cget_exit:

  Nsf_PopFrameObj(interp, framePtr);
  ParamDefsRefCountDecr(paramDefs);

  return result;
}

/*
objectMethod destroy NsfODestroyMethod {
}
*/
static int
NsfODestroyMethod(Tcl_Interp *interp, NsfObject *object) {
  PRINTOBJ("NsfODestroyMethod", object);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*
   * Provide protection against destroy on base classes.
   */
  if (unlikely(IsBaseClass(object))) {
    if (RUNTIME_STATE(interp)->exitHandlerDestroyRound != NSF_EXITHANDLER_ON_SOFT_DESTROY) {
      return NsfPrintError(interp, "cannot destroy base class %s", ObjectName(object));
    }
  }

  /*fprintf(stderr,"NsfODestroyMethod %p %s flags %.6x activation %d cmd %p cmd->flags %.6x\n",
          object, ((Command *)object->id)->flags == 0 ? ObjectName(object) : "(deleted)",
          object->flags, object->activationCount, object->id, ((Command *)object->id)->flags);*/

  /*
   * NSF_DESTROY_CALLED might be set already be DispatchDestroyMethod(),
   * the implicit destroy calls. It is necessary to set it here for
   * the explicit destroy calls in the script, which reach the
   * Object->destroy.
   */

  if ((object->flags & NSF_DESTROY_CALLED) == 0u) {
    object->flags |= NSF_DESTROY_CALLED;
    /*fprintf(stderr, "NsfODestroyMethod %p sets DESTROY_CALLED %.6x\n", object, object->flags);*/
  }
  object->flags |= NSF_DESTROY_CALLED_SUCCESS;

  if (likely((object->flags & NSF_DURING_DELETE) == 0u)) {
    int result;
    Tcl_Obj *methodObj;

    /*fprintf(stderr, "   call dealloc on %p %s\n", object,
      ((Command *)object->id)->flags == 0u ? ObjectName(object) : "(deleted)");*/

    if (CallDirectly(interp, &object->cl->object, NSF_c_dealloc_idx, &methodObj)) {
      NSF_PROFILE_TIME_DATA;
      NSF_PROFILE_CALL(interp, &object->cl->object, Nsf_SystemMethodOpts[NSF_c_dealloc_idx]);
      result = DoDealloc(interp, object);
      NSF_PROFILE_EXIT(interp, &object->cl->object, Nsf_SystemMethodOpts[NSF_c_dealloc_idx]);
    } else {
      result = NsfCallMethodWithArgs(interp, (Nsf_Object *)object->cl, methodObj,
                                     object->cmdName, 1, NULL,
                                     NSF_CSC_IMMEDIATE|NSF_CM_IGNORE_PERMISSIONS);
      if (unlikely(result != TCL_OK)) {
        /*
         * In case, the call of the dealloc method has failed above (e.g. NS_DYING),
         * we have to call dealloc manually, otherwise we have a memory leak
         */
        /*fprintf(stderr, "*** dealloc failed for %p %s flags %.6x, retry\n",
          object, ObjectName(object), object->flags);*/
        result = DoDealloc(interp, object);
      }
    }
    return result;
  } else {
#if defined(OBJDELETION_TRACE)
    fprintf(stderr, "  Object->destroy already during delete, don't call dealloc %p\n", object);
#endif
  }
  return TCL_OK;
}

/*
objectMethod exists NsfOExistsMethod {
  {-argName "varName" -required 1}
}
*/
static int
NsfOExistsMethod(Tcl_Interp *interp, NsfObject *object, const char *var) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(var != NULL);

  Tcl_SetIntObj(Tcl_GetObjResult(interp),
                VarExists(interp, object, var, NULL,
                          NSF_VAR_TRIGGER_TRACE|NSF_VAR_REQUIRE_DEFINED));
  return TCL_OK;
}

/*
objectMethod filterguard NsfOFilterGuardMethod {
  {-argName "filter" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}
*/

static int
NsfOFilterGuardMethod(Tcl_Interp *interp, NsfObject *object, const char *filter, Tcl_Obj *guardObj) {
  NsfObjectOpt *opt;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(filter != NULL);
  nonnull_assert(guardObj != NULL);

  opt = object->opt;
  if (opt != NULL && opt->objFilters) {
    NsfCmdList *h = CmdListFindNameInList(interp, filter, opt->objFilters);
    if (h != NULL) {
      if (h->clientData != NULL) {
        GuardDel((NsfCmdList *) h);
      }
      GuardAdd(h, guardObj);
      object->flags &= ~NSF_FILTER_ORDER_VALID;
      return TCL_OK;
    }
  }

  return NsfPrintError(interp, "filterguard: can't find filter %s on %s",
                       filter, ObjectName(object));
}

/*
objectMethod instvar NsfOInstvarMethod {
  {-argName "args" -type allargs}
}
*/

static int
NsfOInstvarMethod(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[]) {
  callFrameContext ctx = {0, NULL, NULL};
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->filterStack || object->mixinStack) {
    CallStackUseActiveFrame(interp, &ctx);
  }

  if (unlikely(Tcl_Interp_varFramePtr(interp) == NULL)) {
    CallStackRestoreSavedFrames(interp, &ctx);
    return NsfPrintError(interp, "instvar used on %s, but call-stack is not in procedure scope",
                         ObjectName(object));
  }

  result = NsfVarImport(interp, object, ObjStr(objv[0]), objc-1, objv+1);
  CallStackRestoreSavedFrames(interp, &ctx);

  return result;
}

/*
objectMethod mixinguard NsfOMixinGuardMethod {
  {-argName "mixin" -required 1 -type tclobj}
  {-argName "guard" -required 1 -type tclobj}
}
*/

static int
NsfOMixinGuardMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *mixin, Tcl_Obj *guardObj) {
  NsfObjectOpt *opt;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(mixin != NULL);
  nonnull_assert(guardObj != NULL);

  opt = object->opt;
  if (opt != NULL && opt->objMixins) {
    Tcl_Command mixinCmd = Tcl_GetCommandFromObj(interp, mixin);
    if (mixinCmd != NULL) {
      NsfClass *mixinCl = NsfGetClassFromCmdPtr(mixinCmd);
      if (mixinCl != NULL) {
        NsfCmdList *h = CmdListFindCmdInList(mixinCmd, opt->objMixins);
        if (h != NULL) {
          if (h->clientData != NULL) {
            GuardDel((NsfCmdList *) h);
          }
          GuardAdd(h, guardObj);
          object->flags &= ~NSF_MIXIN_ORDER_VALID;
          return TCL_OK;
        }
      }
    }
  }

  return NsfPrintError(interp, "mixinguard: can't find mixin %s on %s",
                       ObjStr(mixin), ObjectName(object));
}

/*
objectMethod noinit NsfONoinitMethod {
}
*/
static int
NsfONoinitMethod(Tcl_Interp *UNUSED(interp), NsfObject *object) {

  nonnull_assert(object != NULL);

  object->flags |= NSF_INIT_CALLED;
  return TCL_OK;
}

/*
objectMethod requirenamespace NsfORequireNamespaceMethod {
}
*/
static int
NsfORequireNamespaceMethod(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  RequireObjNamespace(interp, object);
  return TCL_OK;
}

/*
objectMethod residualargs NsfOResidualargsMethod {
  {-argName "args" -type allargs}
}
*/
static int
NsfOResidualargsMethod(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[]) {
  int i, start = 1, argc, nextArgc, normalArgs, result = TCL_OK, isdasharg = NO_DASH;
  const char *methodName, *nextMethodName, *initString = NULL;
  Tcl_Obj **argv, **nextArgv;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

#if 0
  fprintf(stderr, "NsfOResidualargsMethod %s %2d ", ObjectName(object), objc);
  for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%p %s,", i, &objv[i], ObjStr(objv[i]));}
  fprintf(stderr, "\n");
#endif

  /* skip arguments without leading dash */
  for (i = start; i < objc; i++) {
    if ((isdasharg = IsDashArg(interp, objv[i], 1, &methodName, &argc, &argv))) {
      break;
    }
  }
  normalArgs = i-1;

  /*
   * Get the init string; do it once, outside the loop.  If initString is not
   * obtainable (i.e. not configured in the object system), don't call the
   * "init" method in the loop.
   */
  if (i < objc) {
    NsfObjectSystem *osPtr = GetObjectSystem(object);
    Tcl_Obj *initObj = osPtr->methods[NSF_o_init_idx];

    if (initObj != NULL) {
      initString = ObjStr(initObj);
      assert(initString != NULL);
    }

  }


  for( ; i < objc;  argc = nextArgc, argv = nextArgv, methodName = nextMethodName) {

    Tcl_ResetResult(interp);

    switch (isdasharg) {
    case SKALAR_DASH:    /* Argument is a skalar with a leading dash */
      { int j;

        nextMethodName = NULL;
        nextArgv = NULL;
        nextArgc = 0;

        for (j = i+1; j < objc; j++, argc++) {
          if ((isdasharg = IsDashArg(interp, objv[j], 1, &nextMethodName, &nextArgc, &nextArgv))) {
            break;
          }
        }
        if (initString != NULL) {
          result = CallConfigureMethod(interp, object, initString, methodName, argc+1, objv+i+1);
          if (unlikely(result != TCL_OK)) {
            return result;
          }
        }
        i += argc;
        break;
      }
    case LIST_DASH:  /* Argument is a list with a leading dash, grouping determined by list */
      {        i++;
        nextMethodName = NULL;

        if (i < objc) {
          isdasharg = IsDashArg(interp, objv[i], 1, &nextMethodName, &nextArgc, &nextArgv);
        } else {
          nextMethodName = NULL;
          nextArgv = NULL;
          nextArgc = 0;
        }
        if (initString != NULL) {
          result = CallConfigureMethod(interp, object, initString, methodName, argc+1, argv+1);
          if (unlikely(result != TCL_OK)) {
            return result;
          }
        }
        break;
      }
    default:
      {
        return NsfPrintError(interp, "%s configure: unexpected argument '%s' between parameters",
                             ObjectName(object), ObjStr(objv[i]));
      }
    }
  }

  /*
   * Call init with residual args in case it was not called yet.
   */
  result = DispatchInitMethod(interp, object, normalArgs, objv+1, 0);

  if (likely(result == TCL_OK)) {
    /*
     * Return the non-processed leading arguments unless there was an error
     * (XOTcl convention)
     */
    Tcl_SetObjResult(interp, Tcl_NewListObj(normalArgs, objv+1));
  }

  return result;
}

/*
objectMethod uplevel NsfOUplevelMethod {
  {-argName "args" -type allargs}
}
*/
static int
NsfOUplevelMethod(Tcl_Interp *interp, NsfObject *UNUSED(object), int objc, Tcl_Obj *CONST objv[]) {
  int i, result = TCL_ERROR;
  Tcl_CallFrame *framePtr = NULL, *savedVarFramePtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(objv != NULL);

  /*
   * Find the level to use for executing the command.
   */
  if (objc > 2) {
    CallFrame *cf;
    const char *frameInfo = ObjStr(objv[1]);

    result = TclGetFrame(interp, frameInfo, &cf);
    if (unlikely(result == -1)) {
      return TCL_ERROR;
    }
    framePtr = (Tcl_CallFrame *)cf;
    i = result+1;
  } else {
    i = 1;
  }

  objc -= i;
  objv += i;

  if (framePtr == NULL) {
    NsfCallStackFindLastInvocation(interp, 1, &framePtr);
    if (framePtr == NULL) {
      framePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp)->callerVarPtr;
      if (framePtr == NULL) {
        framePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp);
      }
    }
  }

  savedVarFramePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp);
  Tcl_Interp_varFramePtr(interp) = (CallFrame *)framePtr;

  /*
   * Execute the residual arguments as a command.
   */

  if (objc == 1) {
    result = Tcl_EvalObjEx(interp, objv[0], TCL_EVAL_DIRECT);
  } else {
    /*
     * More than one argument: concatenate them together with spaces
     * between, then evaluate the result.  Tcl_EvalObjEx will delete
     * the object when it decrements its refCount after eval'ing it.
     */
    Tcl_Obj *objPtr = Tcl_ConcatObj(objc, objv);
    result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);
  }
  if (unlikely(result == TCL_ERROR)) {
    Tcl_AppendObjToErrorInfo(interp,
       Tcl_ObjPrintf("\n    (\"uplevel\" body line %d)",
                     Tcl_GetErrorLine(interp)));
  }

  /*
   * Restore the variable frame, and return.
   */

  Tcl_Interp_varFramePtr(interp) = (CallFrame *)savedVarFramePtr;
  return result;
}

/*
objectMethod upvar NsfOUpvarMethod {
  {-argName "args" -type allargs}
}
*/
static int
NsfOUpvarMethod(Tcl_Interp *interp, NsfObject *object, int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *frameInfoObj = NULL;
  int i, result = TCL_ERROR;
  const char *frameInfo;
  callFrameContext ctx = {0, NULL, NULL};

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (objc % 2 == 0) {
    frameInfo = ObjStr(objv[1]);
    i = 2;
  } else {
    frameInfoObj = ComputeLevelObj(interp, CALLING_LEVEL);
    INCR_REF_COUNT(frameInfoObj);
    frameInfo = ObjStr(frameInfoObj);
    i = 1;
  }

  if (object->filterStack || object->mixinStack) {
    CallStackUseActiveFrame(interp, &ctx);
  }

  for ( ;  i < objc;  i += 2) {
    result = Tcl_UpVar2(interp, frameInfo, ObjStr(objv[i]), NULL,
                        ObjStr(objv[i+1]), 0 /*flags*/);
    if (unlikely(result != TCL_OK)) {
      break;
    }
  }

  if (frameInfoObj != NULL) {
    DECR_REF_COUNT(frameInfoObj);
  }
  CallStackRestoreSavedFrames(interp, &ctx);
  return result;
}

/*
objectMethod volatile NsfOVolatileMethod {
}
*/
static int
NsfOVolatileMethod(Tcl_Interp *interp, NsfObject *object) {
  int result = TCL_ERROR;
  Tcl_Obj *objPtr;
  const char *fullName, *vn;
  callFrameContext ctx = {0, NULL, NULL};

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (unlikely(RUNTIME_STATE(interp)->exitHandlerDestroyRound != NSF_EXITHANDLER_OFF)) {
    return NsfPrintError(interp, "can't make objects volatile during shutdown");
  }

  CallStackUseActiveFrame(interp, &ctx);

  objPtr = object->cmdName;
  fullName = ObjStr(objPtr);
  vn = NSTail(fullName);

  if (Tcl_SetVar2(interp, vn, NULL, fullName, 0)) {
    NsfObjectOpt *opt = NsfRequireObjectOpt(object);

    /*fprintf(stderr, "### setting trace for %s on frame %p\n", fullName,
      Tcl_Interp_varFramePtr(interp));
      NsfShowStack(interp);*/
    result = Tcl_TraceVar(interp, vn, TCL_TRACE_UNSETS,
                          (Tcl_VarTraceProc *)NsfUnsetTrace,
                          objPtr);
    opt->volatileVarName = vn;
  }
  CallStackRestoreSavedFrames(interp, &ctx);

  if (likely(result == TCL_OK)) {
    INCR_REF_COUNT(objPtr);
  }
  return result;
}

/***********************************************************************
 * End Object Methods
 ***********************************************************************/


/***********************************************************************
 * Begin Class Methods
 ***********************************************************************/

static int
NsfCAllocMethod_(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj, Tcl_Namespace *parentNsPtr) {
  const char *nameString = ObjStr(nameObj);
  NsfObject *newObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(nameObj != NULL);
  assert(isAbsolutePath(nameString));
  assert(NSValidObjectName(nameString, 0));

  /*
   * Create a new object from scratch.
   */
  if (IsMetaClass(interp, cl, 1) == 0) {
    /*
     * If the base class is an ordinary class, we create an object.
     */
    newObj = PrimitiveOCreate(interp, nameObj, parentNsPtr, cl);
  } else {
    /*
     * If the base class is a meta-class, we create a class.
     */
    newObj = (NsfObject *)PrimitiveCCreate(interp, nameObj, parentNsPtr, cl);
  }

  if (unlikely(newObj == NULL)) {
    return NsfPrintError(interp, "alloc failed to create '%s' "
                         "(possibly parent namespace does not exist)",
                         nameString);
  }

  if (NSF_DTRACE_OBJECT_ALLOC_ENABLED()) {
    NSF_DTRACE_OBJECT_ALLOC(ObjectName(newObj), ClassName(cl));
  }

  Tcl_SetObjResult(interp, nameObj);

  return TCL_OK;
}

/*
classMethod alloc NsfCAllocMethod {
  {-argName "name" -required 1 -type tclobj}
}
*/
static int
NsfCAllocMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj) {
  const char *nameString = ObjStr(nameObj);
  Tcl_Namespace *parentNsPtr;
  Tcl_Obj *tmpName;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(nameObj != NULL);

  /*
   * Create a new object from scratch.
   */

  /*
   * Check for illegal names.
   */
  if (unlikely(NSValidObjectName(nameString, 0) == 0)) {
    return NsfPrintError(interp, "cannot allocate object - illegal name '%s'", nameString);
  }

  /*
   * If the path is not absolute, we add the appropriate namespace.
   */
  if (isAbsolutePath(nameString)) {
    tmpName = NULL;
    parentNsPtr = NULL;
  } else {
    parentNsPtr = CallingNameSpace(interp);
    nameObj = tmpName = NameInNamespaceObj(nameString, parentNsPtr);
    if (strchr(nameString, ':')) {
      parentNsPtr = NULL;
    }
    INCR_REF_COUNT(tmpName);
    /*fprintf(stderr, " **** NoAbsoluteName for '%s' -> determined = '%s' parentNs %s\n",
      nameString, ObjStr(tmpName), parentNsPtr->fullName);*/
  }

  result = NsfCAllocMethod_(interp, cl, nameObj, parentNsPtr);

  if (tmpName != NULL) {
    DECR_REF_COUNT(tmpName);
  }

  return result;
}

/*
classMethod create NsfCCreateMethod {
  {-argName "name" -required 1}
  {-argName "args" -type allargs}
}
*/
static int
NsfCCreateMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *specifiedNameObj, int objc, Tcl_Obj *CONST objv[]) {
  NsfObject *newObject = NULL;
  Tcl_Obj *nameObj, *methodObj, *tmpObj = NULL;
  int result;
  const char *nameString = ObjStr(specifiedNameObj);
  Tcl_Namespace *parentNsPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(specifiedNameObj != NULL);
  nonnull_assert(objv != NULL);

#if 0
  { int i;
    fprintf(stderr, "NsfCCreateMethod %s create <%s> oc %d ", ClassName(cl), ObjStr(specifiedNameObj), objc);
    for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
    fprintf(stderr, "\n");
  }
#endif

  if (unlikely(RUNTIME_STATE(interp)->exitHandlerDestroyRound != NSF_EXITHANDLER_OFF)) {
    fprintf(stderr, "### Can't create object %s during shutdown\n", ObjStr(objv[1]));
    return TCL_OK; /* don't fail, if this happens during destroy, it might be canceled */
  }

  /*
   * Check for illegal names.
   */
  if (unlikely(NSValidObjectName(nameString, 0) == 0)) {
    result = NsfPrintError(interp, "cannot allocate object - illegal name '%s'", nameString);
    goto create_method_exit;
  }

  /*fprintf(stderr, "NsfCCreateMethod specifiedName %s\n", nameString);*/
  /*
   * Complete the name if it is not absolute.
   */
  if (!isAbsolutePath(nameString)) {
    parentNsPtr = CallingNameSpace(interp);
    tmpObj = NameInNamespaceObj(nameString, parentNsPtr);
    /*
     * If the name contains colons, the parentNsPtr is not appropriate
     * for determining the parent.
     */
    if (strchr(nameString, ':')) {
      parentNsPtr = NULL;
    }
    nameString = ObjStr(tmpObj);
    /* fprintf(stderr, " **** fixed name is '%s'\n", nameString); */
    INCR_REF_COUNT(tmpObj);
    nameObj = tmpObj;

  } else {
    parentNsPtr = NULL;
    nameObj = specifiedNameObj;
    /* fprintf(stderr, " **** used specified name is '%s'\n", nameString); */
  }

  /*
   * Check whether we have to call recreate (i.e. when the object exists
   * already). First check, if was have such a command, then check, if the
   * command is an object.
   */
  {
    Tcl_Command cmd = NSFindCommand(interp, nameString);
    if (cmd != NULL) {
      newObject = NsfGetObjectFromCmdPtr(cmd);
      if (newObject == NULL) {
        /*
         * We have a cmd, but no object. Don't allow to overwrite an ordinary
         * cmd by an nsf object.
         */
        result = NsfPrintError(interp, "refuse to overwrite cmd %s; delete/rename it before overwriting", nameString);
        goto create_method_exit;
      }
    }
  }

  /*fprintf(stderr, "+++ createspecifiedName '%s', nameString '%s', newObject=%p ismeta(%s) %d, ismeta(%s) %d\n",
          ObjStr(specifiedNameObj), nameString, newObject,
          ClassName(cl), IsMetaClass(interp, cl, 1), (newObject != NULL) ? ClassName(newObject->cl) : "NULL", (newObject != NULL) ? IsMetaClass(interp, newObject->cl, 1) : 0
          );*/

  /*
   * Provide protection against recreation if base classes.
   */
  if (unlikely(newObject != NULL && unlikely(IsBaseClass(newObject)))) {
    result = NsfPrintError(interp, "cannot recreate base class %s", ObjectName(newObject));
    goto create_method_exit;
  }

  /*
   * Don't allow to
   *  - recreate an object as a class,
   *  - recreate a class as an object, and to
   *  - recreate an object in a different object system
   *
   * In these clases, we use destroy followed by create instead of recreate.
   */

  if (newObject
      && (IsMetaClass(interp, cl, 1) == IsMetaClass(interp, newObject->cl, 1))
      && GetObjectSystem(newObject) == cl->osPtr) {

    /*fprintf(stderr, "%%%% recreate, call recreate method ... %s, objc=%d oldOs %p != newOs %p EQ %d\n",
            ObjStr(nameObj), objc+1,
            GetObjectSystem(newObject), cl->osPtr,
            GetObjectSystem(newObject) != cl->osPtr
            );*/


    /* call recreate --> initialization */
    if (CallDirectly(interp, &cl->object, NSF_c_recreate_idx, &methodObj)) {
      NSF_PROFILE_TIME_DATA;
      NSF_PROFILE_CALL(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_recreate_idx]);
      result = RecreateObject(interp, cl, newObject, objc, objv);
      NSF_PROFILE_EXIT(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_recreate_idx]);
    } else {

      ALLOC_ON_STACK(Tcl_Obj*, objc+3, xov);

      xov[0] = NULL; /* just a placeholder for passing conventions in ObjectDispatch() */
      xov[1] = methodObj;
      xov[2] = nameObj;
      if (objc >= 1) {
        memcpy(xov+3, objv, sizeof(Tcl_Obj *)*objc);
      }
      result = ObjectDispatch(cl, interp, objc+3, xov, NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE);

      FREE_ON_STACK(Tcl_Obj *, xov);
    }

    if (unlikely(result != TCL_OK)) {
      goto create_method_exit;
    }

    Tcl_SetObjResult(interp, newObject->cmdName);
    nameObj = newObject->cmdName;
    ObjTrace("RECREATE", newObject);

  } else {
    /*
     * newObject might exist here, but will be automatically destroyed by
     * alloc.
     */

    if (CallDirectly(interp, &cl->object, NSF_c_alloc_idx, &methodObj)) {
      NSF_PROFILE_TIME_DATA;
      NSF_PROFILE_CALL(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_alloc_idx]);
      result = NsfCAllocMethod_(interp, cl, nameObj, parentNsPtr);
      NSF_PROFILE_EXIT(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_alloc_idx]);
    } else {
      result = CallMethod(cl, interp, methodObj,
                          3, &nameObj, NSF_CSC_IMMEDIATE);
    }

    if (unlikely(result != TCL_OK)) {
      goto create_method_exit;
    }
    nameObj = Tcl_GetObjResult(interp);

    if (unlikely(GetObjectFromObj(interp, nameObj, &newObject) != TCL_OK)) {
      result = NsfPrintError(interp, "couldn't find result of alloc");
      goto create_method_exit;
    }

    ObjTrace("CREATE", newObject);

    /*
     * In case, the object is destroyed during initialization, we incr
     * refCount.
     */
    INCR_REF_COUNT(nameObj);
    result = DoObjInitialization(interp, newObject, objc, objv);
    DECR_REF_COUNT(nameObj);
  }
 create_method_exit:

  if (tmpObj != NULL) {DECR_REF_COUNT(tmpObj);}
  return result;
}

/*
classMethod dealloc NsfCDeallocMethod {
  {-argName "object" -required 1 -type tclobj}
}
*/

static int
NsfCDeallocMethod(Tcl_Interp *interp, NsfClass *UNUSED(cl), Tcl_Obj *obj) {
  NsfObject *object;

  nonnull_assert(interp != NULL);

  if (GetObjectFromObj(interp, obj, &object) != TCL_OK) {
    return NsfPrintError(interp, "can't destroy object %s that does not exist",
                         ObjStr(obj));
  }

  return DoDealloc(interp, object);
}

/*
classMethod filterguard NsfCFilterGuardMethod {
  {-argName "filter" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}
*/

static int
NsfCFilterGuardMethod(Tcl_Interp *interp, NsfClass *cl,
                      const char *filter, Tcl_Obj *guardObj) {
  NsfClassOpt *opt;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(filter != NULL);
  nonnull_assert(guardObj != NULL);

  opt = cl->opt;
  if (opt != NULL && opt->classFilters) {
    NsfCmdList *h = CmdListFindNameInList(interp, filter, opt->classFilters);

    if (h != NULL) {
      NsfClasses *subClasses = DependentSubClasses(cl);

      if (h->clientData != NULL) {
        GuardDel(h);
      }
      GuardAdd(h, guardObj);

      if (subClasses != NULL) {
        FilterInvalidateObjOrders(interp, subClasses);
        NsfClassListFree(subClasses);
      }

      return TCL_OK;
    }
  }

  return NsfPrintError(interp, "filterguard: can't find filter %s on %s",
                       filter, ClassName(cl));
}

/*
classMethod getCachedParameters NsfCGetCachendParametersMethod {
}
*/
static int
NsfCGetCachendParametersMethod(Tcl_Interp *interp, NsfClass *class) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  if (likely(class->parsedParamPtr != NULL && class->parsedParamPtr->paramDefs != NULL)) {
    Tcl_Obj *listObj;

    listObj = ListParamDefs(interp, class->parsedParamPtr->paramDefs->paramsPtr,
                            NULL, NULL, NSF_PARAMS_PARAMETER);
    Tcl_SetObjResult(interp, listObj);
    DECR_REF_COUNT2("paramDefsObj", listObj);
  }
  return TCL_OK;
}

/*
classMethod mixinguard NsfCMixinGuardMethod {
  {-argName "mixin" -required 1 -type tclobj}
  {-argName "guard" -required 1 -type tclobj}
}
*/
static int
NsfCMixinGuardMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *mixin, Tcl_Obj *guardObj) {
  NsfClassOpt *opt;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(mixin != NULL);
  nonnull_assert(guardObj != NULL);

  opt = cl->opt;
  if (opt != NULL && opt->classMixins != NULL) {
    Tcl_Command mixinCmd = Tcl_GetCommandFromObj(interp, mixin);

    if (mixinCmd != NULL) {
      NsfClass *mixinCl = NsfGetClassFromCmdPtr(mixinCmd);

      if (mixinCl != NULL) {
        NsfCmdList *h = CmdListFindCmdInList(mixinCmd, opt->classMixins);

        if (h != NULL) {
          NsfClasses *subClasses;

          if (h->clientData != NULL) {
            GuardDel((NsfCmdList *) h);
          }
          GuardAdd(h, guardObj);
          subClasses = DependentSubClasses(cl);
          MixinInvalidateObjOrders(interp, cl, subClasses);
          NsfClassListFree(subClasses);
          return TCL_OK;
        }
      }
    }
  }

  return NsfPrintError(interp, "mixinguard: can't find mixin %s on %s",
                       ObjStr(mixin), ClassName(cl));
}

/*
classMethod new NsfCNewMethod {
  {-argName "-childof" -required 0 -type tclobj}
  {-argName "args" -required 0 -type args}
}
*/

static int
NsfCNewMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *withChildof,
              int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *fullnameObj;
  Tcl_DString dFullname, *dsPtr = &dFullname;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

#if 0
  { int i;
    fprintf(stderr, "NsfCNewMethod %s withChildof %p oc %d ", ClassName(cl), withChildof, objc);
    for(i = 0; i < objc; i++) {fprintf(stderr, " [%d]=%s,", i, ObjStr(objv[i]));}
    fprintf(stderr, "\n");
  }
#endif

  Tcl_DStringInit(dsPtr);
  if (withChildof != 0) {
    const char *parentName = ObjStr(withChildof);

    /*
     * If parentName is fully qualified, use it as prefix, else prepend the
     * CallingNameSpace() to be compatible with the object name completion.
     */
    if (*parentName == ':' && *(parentName + 1) == ':') {
      /*
       * Prepend parentName only if it is not "::"
       */
      if (*(parentName + 2) != '\0') {
        Tcl_DStringAppend(dsPtr, parentName, -1);
      }
    } else {
      Tcl_Obj *tmpName = NameInNamespaceObj(parentName, CallingNameSpace(interp));
      const char *completedParentName;

      INCR_REF_COUNT(tmpName);
      completedParentName = ObjStr(tmpName);
      if (strcmp(completedParentName, "::")) {
        Tcl_DStringAppend(dsPtr, ObjStr(tmpName), -1);
      }
      DECR_REF_COUNT(tmpName);
    }
    Tcl_DStringAppend(dsPtr, "::__#", 5);
  } else {
    Tcl_DStringAppend(dsPtr, "::nsf::__#", 10);
  }

  NewTclCommand(interp, dsPtr);

  fullnameObj = Tcl_NewStringObj(Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));
  INCR_REF_COUNT(fullnameObj);

  {
    Tcl_Obj *methodObj;
    int callDirectly;

    callDirectly = CallDirectly(interp, &cl->object, NSF_c_create_idx, &methodObj);

    if (callDirectly != 0) {
      NSF_PROFILE_TIME_DATA;
      NSF_PROFILE_CALL(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_create_idx]);
      result = NsfCCreateMethod(interp, cl, fullnameObj, objc, objv);
      NSF_PROFILE_EXIT(interp, &cl->object, Nsf_SystemMethodOpts[NSF_c_create_idx]);
    } else {
      ALLOC_ON_STACK(Tcl_Obj*, objc+3, ov);

      ov[0] = NULL; /* just a placeholder for passing conventions in ObjectDispatch() */
      ov[1] = methodObj;
      ov[2] = fullnameObj;
      if (objc >= 1) {
        memcpy(ov+3, objv, sizeof(Tcl_Obj *)*objc);
      }
      result = ObjectDispatch(cl, interp, objc+3, ov, NSF_CSC_IMMEDIATE);
      FREE_ON_STACK(Tcl_Obj *, ov);
    }
  }

  DECR_REF_COUNT(fullnameObj);
  Tcl_DStringFree(dsPtr);

  return result;
}

/*
classMethod recreate NsfCRecreateMethod {
  {-argName "name" -required 1 -type tclobj}
  {-argName "args" -type allargs}
}
*/
static int
RecreateObject(Tcl_Interp *interp, NsfClass *class, NsfObject *object,
               int objc, Tcl_Obj *CONST objv[]) {
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(objv != NULL);

  object->flags |= NSF_RECREATE;

  /*
   * First, cleanup the data from the object.
   *
   * Check whether we have a pending destroy on the object; if yes,
   * clear it, such that the recreated object and won't be destroyed
   * on a POP.
   */
  MarkUndestroyed(object);

  /*
   * Ensure correct class for object.
   */
  result = ChangeClass(interp, object, class);

  if (likely(result == TCL_OK)) {
    Tcl_Obj *methodObj;

    /*
     * Dispatch "cleanup" method.
     */
    if (CallDirectly(interp, object, NSF_o_cleanup_idx, &methodObj)) {
      NSF_PROFILE_TIME_DATA;
      /*fprintf(stderr, "RECREATE calls cleanup directly for object %s\n", ObjectName(object));*/
      NSF_PROFILE_CALL(interp, object, Nsf_SystemMethodOpts[NSF_o_cleanup_idx]);
      result = NsfOCleanupMethod(interp, object);
      NSF_PROFILE_EXIT(interp, object, Nsf_SystemMethodOpts[NSF_o_cleanup_idx]);
    } else {
      /*NsfObjectSystem *osPtr = GetObjectSystem(object);
      fprintf(stderr, "RECREATE calls method cleanup for object %p %s OS %s\n",
              object, ObjectName(object), ObjectName(&osPtr->rootClass->object));*/
      result = CallMethod(object, interp, methodObj,
                          2, NULL, NSF_CM_IGNORE_PERMISSIONS|NSF_CSC_IMMEDIATE);
    }
  }

  /*
   * Second: if cleanup was successful, initialize the object as usual.
   */
  if (likely(result == TCL_OK)) {
    result = DoObjInitialization(interp, object, objc, objv);
    if (likely(result == TCL_OK)) {
      Tcl_SetObjResult(interp, object->cmdName);
    } else {
      /* fprintf(stderr, "recreate DoObjInitialization returned %d\n", result);*/
    }
  }
  return result;
}

static int
NsfCRecreateMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *nameObj,
                   int objc, Tcl_Obj *CONST objv[]) {
  NsfObject *object;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);
  nonnull_assert(nameObj != NULL);

  if (GetObjectFromObj(interp, nameObj, &object) != TCL_OK) {
    return NsfPrintError(interp, "can't recreate non existing object %s", ObjStr(nameObj));
  }
  return RecreateObject(interp, cl, object, objc, objv);
}

/*
classMethod superclass NsfCSuperclassMethod {
  {-argName "superclasses" -required 0 -type tclobj}
}
*/
static int
NsfCSuperclassMethod(Tcl_Interp *interp, NsfClass *cl, Tcl_Obj *superClassesObj) {

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  return NsfRelationSetCmd(interp, &cl->object, RelationtypeSuperclassIdx, superClassesObj);
}

/***********************************************************************
 * End Class Methods
 ***********************************************************************/

static int
AggregatedMethodType(int methodType) {
  switch (methodType) {
  case MethodtypeNULL: /* default */
  case MethodtypeAllIdx:
    methodType = NSF_METHODTYPE_ALL;
    break;
  case MethodtypeScriptedIdx:
    /*methodType = NSF_METHODTYPE_SCRIPTED|NSF_METHODTYPE_ALIAS;*/
    methodType = NSF_METHODTYPE_SCRIPTED;
    break;
  case MethodtypeBuiltinIdx:
    methodType = NSF_METHODTYPE_BUILTIN|NSF_METHODTYPE_OBJECT;
    break;
  case MethodtypeForwarderIdx:
    methodType = NSF_METHODTYPE_FORWARDER;
    break;
  case MethodtypeAliasIdx:
    methodType = NSF_METHODTYPE_ALIAS;
    break;
  case MethodtypeSetterIdx:
    methodType = NSF_METHODTYPE_SETTER;
    break;
  case MethodtypeObjectIdx:
    methodType = NSF_METHODTYPE_OBJECT;
    break;
  case MethodtypeNsfprocIdx:
    methodType = NSF_METHODTYPE_NSFPROC;
    break;
  default:
    methodType = 0;
  }

  return methodType;
}

/***********************************************************************
 * Begin Object Info Methods
 ***********************************************************************/

/*
objectInfoMethod children NsfObjInfoChildrenMethod {
  {-argName "-type" -required 0 -nrargs 1 -type class}
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoChildrenMethod(Tcl_Interp *interp, NsfObject *object, NsfClass *type, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return ListChildren(interp, object, pattern, 0, type);
}

/*
objectInfoMethod class NsfObjInfoClassMethod {
}
*/
static int
NsfObjInfoClassMethod(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  Tcl_SetObjResult(interp, object->cl->object.cmdName);
  return TCL_OK;
}

/*
objectInfoMethod filterguard NsfObjInfoFilterguardMethod {
  {-argName "filter" -required 1}
}
*/
static int
NsfObjInfoFilterguardMethod(Tcl_Interp *interp, NsfObject *object, const char *filter) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(filter != NULL);

  return (object->opt != NULL) ? GuardList(interp, object->opt->objFilters, filter) : TCL_OK;
}

/*
objectInfoMethod filters NsfObjInfoFiltersMethod {
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "pattern"}
}
*/
static int
NsfObjInfoFiltersMethod(Tcl_Interp *interp, NsfObject *object, int withGuards,
                        const char *pattern) {
  NsfObjectOpt *opt;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  opt = object->opt;
  return (opt != NULL) ? FilterInfo(interp, opt->objFilters, pattern, withGuards, 0) : TCL_OK;
}

/*
objectInfoMethod forward NsfObjInfoForwardMethod {
  {-argName "-definition"}
  {-argName "name"}
}
*/
static int
NsfObjInfoForwardMethod(Tcl_Interp *interp, NsfObject *object, int withDefinition, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return (object->nsPtr != NULL) ?
    ListForward(interp, Tcl_Namespace_cmdTablePtr(object->nsPtr), pattern, withDefinition) :
    TCL_OK;
}

/*
objectInfoMethod hasmixin NsfObjInfoHasMixinMethod {
  {-argName "class" -required 1 -type class}
}
*/
static int
NsfObjInfoHasMixinMethod(Tcl_Interp *interp, NsfObject *object, NsfClass *mixinClass) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(mixinClass != NULL);

  Tcl_SetBooleanObj(Tcl_GetObjResult(interp), HasMixin(interp, object, mixinClass));
  return TCL_OK;
}

/*
objectInfoMethod hasnamespace NsfObjInfoHasnamespaceMethod {
}
*/
static int
NsfObjInfoHasnamespaceMethod(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  Tcl_SetBooleanObj(Tcl_GetObjResult(interp), object->nsPtr != NULL);
  return TCL_OK;
}

/*
objectInfoMethod hastype NsfObjInfoHasTypeMethod {
  {-argName "class" -required 1 -type class}
}
*/
static int
NsfObjInfoHasTypeMethod(Tcl_Interp *interp, NsfObject *object, NsfClass *typeClass) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(typeClass != NULL);

  Tcl_SetBooleanObj(Tcl_GetObjResult(interp), IsSubType(object->cl, typeClass));
  return TCL_OK;
}

/*
objectInfoMethod lookupfilter NsfObjInfoLookupFilterMethod {
  {-argName "filter" -required 1}
}
*/
static int
NsfObjInfoLookupFilterMethod(Tcl_Interp *interp, NsfObject *object, const char *filter) {
  const char *filterName;
  NsfCmdList *cmdList;
  NsfClass *fcl;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(filter != NULL);

  /*
   * Searches for filter on [self] and returns fully qualified name if it is
   * not found it returns an empty string.
   */
  Tcl_ResetResult(interp);

  if ((object->flags & NSF_FILTER_ORDER_VALID) == 0u) {
    FilterComputeDefined(interp, object);
  }
  if ((object->flags & NSF_FILTER_ORDER_DEFINED) == 0u) {
    return TCL_OK;
  }
  for (cmdList = object->filterOrder; cmdList;  cmdList = cmdList->nextPtr) {
    filterName = Tcl_GetCommandName(interp, cmdList->cmdPtr);
    if (filterName[0] == filter[0] && !strcmp(filterName, filter)) {
      break;
    }
  }

  if (cmdList == NULL) {
    return TCL_OK;
  }
  fcl = cmdList->clorobj;
  Tcl_SetObjResult(interp, MethodHandleObj((NsfObject *)fcl, !NsfObjectIsClass(&fcl->object), filterName));
  return TCL_OK;
}


/*
objectInfoMethod lookupfilters NsfObjInfoLookupFiltersMethod {
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "pattern"}
}
*/
static int
NsfObjInfoLookupFiltersMethod(Tcl_Interp *interp, NsfObject *object, int withGuards, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if ((object->flags & NSF_FILTER_ORDER_VALID) == 0u) {
    FilterComputeDefined(interp, object);
  }
  return FilterInfo(interp, object->filterOrder, pattern, withGuards, 1);
}

/*
objectInfoMethod lookupmethod NsfObjInfoLookupMethodMethod {
  {-argName "name" -required 1 -type tclobj}
}
*/
static int
NsfObjInfoLookupMethodMethod(Tcl_Interp *interp, NsfObject *object, Tcl_Obj *methodObj) {
  NsfClass *pcl = NULL;
  Tcl_Command cmd;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(methodObj != NULL);

  cmd = ObjectFindMethod(interp, object, methodObj, &pcl);
  if (likely(cmd != NULL)) {
    NsfObject *pobj = (pcl != NULL) ? &pcl->object : object;
    int perObject = (pcl == NULL);

    ListMethod(interp, pobj, pobj, ObjStr(methodObj), cmd,
               InfomethodsubcmdRegistrationhandleIdx,
               NULL, NULL, perObject);
  }
  return TCL_OK;
}


/*
objectInfoMethod lookupmethods NsfObjInfoLookupMethodsMethod {
  {-argName "-callprotection" -nrargs 1 -type "all|public|protected|private" -default all}
  {-argName "-incontext"}
  {-argName "-type" -nrargs 1 -typeName "methodtype" -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-nomixins"}
  {-argName "-path" -nrargs 0}
  {-argName "-source" -nrargs 1 -type "all|application|system"}
  {-argName "pattern" -required 0}
}
*/
static int ListMethodKeysClassList(Tcl_Interp *interp, NsfClasses *classListPtr,
                        int withSource, const char *pattern,
                        int methodType, int withCallprotection,
                        int withPath, Tcl_HashTable *dups,
                        NsfObject *object, int withPer_object)
  nonnull(1) nonnull(8) nonnull(9);

static int
ListMethodKeysClassList(Tcl_Interp *interp, NsfClasses *classListPtr,
                        int withSource, const char *pattern,
                        int methodType, int withCallprotection,
                        int withPath, Tcl_HashTable *dups,
                        NsfObject *object, int withPer_object) {
  nonnull_assert(interp != NULL);
  nonnull_assert(dups != NULL);
  nonnull_assert(object != NULL);

  /* append method keys from inheritance order */
  for (; classListPtr != NULL; classListPtr = classListPtr->nextPtr) {
    Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(classListPtr->cl->nsPtr);

    if (!MethodSourceMatches(withSource, classListPtr->cl, NULL)) {
      continue;
    }

    ListMethodKeys(interp, cmdTablePtr, NULL, pattern, methodType,
                   withCallprotection, withPath,
                   dups, object, withPer_object);
  }
  return TCL_OK;
}

/*
objectInfoMethod lookupmethods NsfObjInfoLookupMethodsMethod {
  {-argName "-callprotection" -type "all|public|protected|private" -default all}
  {-argName "-incontext" -nrargs 0}
  {-argName "-type" -typeName "methodtype" -type "all|scripted|builtin|alias|forwarder|object|setter|nsfproc"}
  {-argName "-nomixins" -nrargs 0}
  {-argName "-path" -nrargs 0}
  {-argName "-source" -type "all|application|system" -default all}
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoLookupMethodsMethod(Tcl_Interp *interp, NsfObject *object,
                              int withCallprotection,
                              int withIncontext,
                              int withMethodtype,
                              int withNomixins,
                              int withPath,
                              int withSource,
                              const char *pattern) {
  int withPer_object = 1;
  Tcl_HashTable dupsTable, *dups = &dupsTable;
  int result, methodType = AggregatedMethodType(withMethodtype);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*
   * TODO: we could make this faster for patterns without meta-chars
   * by letting ListMethodKeys() to signal us when an entry was found.
   * we wait, until the we decided about "info methods defined"
   * vs. "info method search" vs. "info defined" etc.
   */
  if (withCallprotection == CallprotectionNULL) {
    withCallprotection = CallprotectionPublicIdx;
  }
  if (withSource == SourceNULL) {
    withSource = SourceAllIdx;
  }

  Tcl_InitHashTable(dups, TCL_STRING_KEYS);
  if (object->nsPtr != NULL) {
     Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(object->nsPtr);
    if (MethodSourceMatches(withSource, NULL, object)) {
      ListMethodKeys(interp, cmdTablePtr, NULL, pattern, methodType,
                     withCallprotection, withPath,
                     dups, object, withPer_object);
    }
  }

  if (withNomixins == 0) {
    if ((object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
      MixinComputeDefined(interp, object);
    }
    if ((object->flags & NSF_MIXIN_ORDER_DEFINED_AND_VALID) != 0u) {
      NsfCmdList *ml;

      for (ml = object->mixinOrder; ml; ml = ml->nextPtr) {
        int guardOk = TCL_OK;
        NsfClass *mixin = NsfGetClassFromCmdPtr(ml->cmdPtr);

        assert(mixin != NULL);
        if (withIncontext != 0) {
          if (!RUNTIME_STATE(interp)->guardCount && ml->clientData) {
            guardOk = GuardCall(object, interp, ml->clientData, NULL);
          }
        }
        if (mixin && guardOk == TCL_OK) {
          Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(mixin->nsPtr);
          if (!MethodSourceMatches(withSource, mixin, NULL)) {
            continue;
          }
          ListMethodKeys(interp, cmdTablePtr, NULL, pattern, methodType,
                         withCallprotection, withPath,
                         dups, object, withPer_object);
        }
      }
    }
  }

  result = ListMethodKeysClassList(interp, PrecedenceOrder(object->cl),
                                   withSource, pattern,
                                   methodType, withCallprotection,
                                   withPath, dups, object, withPer_object);

  Tcl_DeleteHashTable(dups);
  return result;
}

/*
objectInfoMethod lookupmixins NsfObjInfoLookupMixinsMethod {
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfObjInfoLookupMixinsMethod(Tcl_Interp *interp, NsfObject *object, int withGuards,
                             const char *patternString, NsfObject *patternObj) {
  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if ((object->flags & NSF_MIXIN_ORDER_VALID) == 0u) {
    MixinComputeDefined(interp, object);
  }
  return MixinInfo(interp, object->mixinOrder, patternString, withGuards, patternObj);
}


/*
objectInfoMethod lookupslots NsfObjInfoLookupSlotsMethod {
  {-argName "-source" -nrargs 1 -type "all|application|system" -default all}
  {-argName "-type" -required 0 -nrargs 1 -type class}
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoLookupSlotsMethod(Tcl_Interp *interp, NsfObject *object,
                            int withSource, NsfClass *type,
                            const char *pattern) {
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
  NsfClasses *precendenceList, *clPtr;
  Tcl_HashTable slotTable;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  precendenceList = ComputePrecedenceList(interp, object, NULL /* pattern*/, 1, 1);
  assert(precendenceList != NULL);

  if (withSource == 0) {withSource = 1;}

  Tcl_InitHashTable(&slotTable, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable", &slotTable);

  /*
   * First add the per-object slot objects.
   */
  if (MethodSourceMatches(withSource, NULL, object)) {
    AddSlotObjects(interp, object, "::per-object-slot", &slotTable,
                   withSource, type, pattern, listObj);
  }

  /*
   * Then add the class provided slot objects.
   */
  for (clPtr = precendenceList; likely(clPtr != NULL); clPtr = clPtr->nextPtr) {
    if (MethodSourceMatches(withSource, clPtr->cl, NULL)) {
      AddSlotObjects(interp, &clPtr->cl->object, "::slot", &slotTable,
                     withSource, type, pattern, listObj);
    }
  }

  Tcl_DeleteHashTable(&slotTable);
  MEM_COUNT_FREE("Tcl_InitHashTable", &slotTable);

  NsfClassListFree(precendenceList);
  Tcl_SetObjResult(interp, listObj);

  return TCL_OK;
}

/*
objectInfoMethod method NsfObjInfoMethodMethod {
  {-argName "infomethodsubcmd" -type "args|body|definition|exists|registrationhandle|definitionhandle|origin|parameter|syntax|type|precondition|postcondition|submethods"}
  {-argName "name" -required 1 -type tclobj}
}
*/
static int
NsfObjInfoMethodMethod(Tcl_Interp *interp, NsfObject *object,
                       int subcmd, Tcl_Obj *methodNameObj) {
  return ListMethodResolve(interp, subcmd, NULL, NULL, object->nsPtr, object, methodNameObj, 0);
}

/*
objectInfoMethod methods NsfObjInfoMethodsMethod {
  {-argName "-callprotection" -type "all|public|protected|private" -default all}
  {-argName "-type" -nrargs 1 -typeName "methodtype" -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-path" -nrargs 0}
  {-argName "pattern"}
}
*/
static int
NsfObjInfoMethodsMethod(Tcl_Interp *interp, NsfObject *object,
                        int withCallproctection,
                        int withMethodtype,
                        int withPath,
                        const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return ListDefinedMethods(interp, object, pattern, 1 /* per-object */,
                            AggregatedMethodType(withMethodtype), withCallproctection,
                            withPath);
}

/*
objectInfoMethod mixins NsfObjInfoMixinsMethod {
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfObjInfoMixinsMethod(Tcl_Interp *interp, NsfObject *object, int withGuards,
                       const char *patternString, NsfObject *patternObj) {
  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  return (object->opt != NULL) ?
    MixinInfo(interp, object->opt->objMixins, patternString, withGuards, patternObj) :
    TCL_OK;
}

/*
objectInfoMethod mixinguard NsfObjInfoMixinguardMethod {
  {-argName "mixin"  -required 1}
}
*/
static int
NsfObjInfoMixinguardMethod(Tcl_Interp *interp, NsfObject *object, const char *mixin) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);
  nonnull_assert(mixin != NULL);

  return (object->opt != NULL) ? GuardList(interp, object->opt->objMixins, mixin) : TCL_OK;
}

/*
objectInfoMethod name NsfObjInfoNameMethod {
}
*/
static int
NsfObjInfoNameMethod(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  Tcl_SetObjResult(interp,  Tcl_NewStringObj(Tcl_GetCommandName(interp, object->id), -1));
  return TCL_OK;
}

/*
objectInfoMethod objectparameter NsfObjInfoObjectparameterMethod {
  {-argName "infoobjectparametersubcmd" -type "definitions|list|names|syntax" -required 1}
  {-argName "pattern" -required 0}
}
*/
/*
 * Actually, this method (object::info::objectparameter) is not used anymore.
 */
static int
NsfObjInfoObjectparameterMethod(Tcl_Interp *interp, NsfObject *object, int subcmd, const char *pattern) {
  NsfParsedParam parsedParam;
  Tcl_Obj *listObj = NULL;
  Nsf_Param const *paramsPtr;
  Nsf_Param *paramList = NULL;
  int result;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  result = GetObjectParameterDefinition(interp, NsfGlobalObjs[NSF_EMPTY],
                                        object, NULL, &parsedParam);

  if (result != TCL_OK || parsedParam.paramDefs == NULL) {
    return result;
  }

  paramsPtr = parsedParam.paramDefs->paramsPtr;

  if (pattern != NULL) {
    paramList = NsfParamDefsFilter(interp, paramsPtr, pattern);
    if (unlikely(paramList == NULL)) {
      /*
       * The named parameter were NOT found, so return "".
       */
      Tcl_SetObjResult(interp, NsfGlobalObjs[NSF_EMPTY]);
      return TCL_OK;
    }
    /* Iterate below over the computed selection */
    paramsPtr = paramList;
  }

  switch (subcmd) {
  case InfoobjectparametersubcmdDefinitionsIdx:
    listObj = ParamDefsFormat(interp, paramsPtr, NULL, NULL);
    break;
  case InfoobjectparametersubcmdListIdx:
    listObj = ParamDefsList(interp, paramsPtr, NULL, NULL);
    break;
  case InfoobjectparametersubcmdNamesIdx:
    listObj = ParamDefsNames(interp, paramsPtr, NULL, NULL);
    break;
  case InfoobjectparametersubcmdSyntaxIdx:
    listObj = NsfParamDefsSyntax(interp, paramsPtr, NULL, NULL);
    break;
  }
  assert(listObj != NULL);

  Tcl_SetObjResult(interp, listObj);

  DECR_REF_COUNT2("paramDefsObj", listObj);
  if (paramList != NULL) {
    FREE(Nsf_Param*, paramList);
  }

  return TCL_OK;
}

/*
objectInfoMethod parent NsfObjInfoParentMethod {
}
*/
static int
NsfObjInfoParentMethod(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  if (object->id != NULL) {
    Tcl_Namespace *nsPtr = Tcl_Command_nsPtr(object->id);
    Tcl_SetObjResult(interp, Tcl_NewStringObj((nsPtr != NULL) ? nsPtr->fullName : "", -1));
  }
  return TCL_OK;
}

/*
objectInfoMethod precedence NsfObjInfoPrecedenceMethod {
  {-argName "-intrinsic"}
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoPrecedenceMethod(Tcl_Interp *interp, NsfObject *object,
                                        int withIntrinsicOnly, const char *pattern) {
  NsfClasses *precedenceList, *pl;
  Tcl_Obj *resultObj = Tcl_NewObj();

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  precedenceList = ComputePrecedenceList(interp, object, pattern, !withIntrinsicOnly, 1);
  for (pl = precedenceList; pl; pl = pl->nextPtr) {
    assert(pl->cl);
    Tcl_ListObjAppendElement(interp, resultObj, pl->cl->object.cmdName);
  }
  if (precedenceList != NULL) {
    NsfClassListFree(precedenceList);
  }

  Tcl_SetObjResult(interp, resultObj);
  return TCL_OK;
}

/*
objectInfoMethod slotobjects NsfObjInfoSlotobjectsMethod {
  {-argName "-type" -required 0 -nrargs 1 -type class}
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoSlotobjectsMethod(Tcl_Interp *interp, NsfObject *object,
                      NsfClass *type, const char *pattern) {
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  AddSlotObjects(interp, object, "::per-object-slot", NULL,
                 SourceAllIdx, type, pattern, listObj);

  Tcl_SetObjResult(interp, listObj);
  return TCL_OK;
}


/*
objectInfoMethod vars NsfObjInfoVarsMethod {
  {-argName "pattern" -required 0}
}
*/
static int
NsfObjInfoVarsMethod(Tcl_Interp *interp, NsfObject *object, const char *pattern) {
  Tcl_Obj *varList, *okList, *element;
  int i, length;
  TclVarHashTable *varTablePtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  varTablePtr = (object->nsPtr != NULL) ?
    Tcl_Namespace_varTablePtr(object->nsPtr) :
    object->varTablePtr;

  ListVarKeys(interp, TclVarHashTablePtr(varTablePtr), pattern);
  varList = Tcl_GetObjResult(interp);

  Tcl_ListObjLength(interp, varList, &length);
  okList = Tcl_NewListObj(0, NULL);
  for (i = 0; i < length; i++) {
    Tcl_ListObjIndex(interp, varList, i, &element);
    if (VarExists(interp, object, ObjStr(element), NULL, NSF_VAR_REQUIRE_DEFINED)) {
      Tcl_ListObjAppendElement(interp, okList, element);
    } else {
      /*fprintf(stderr, "must ignore '%s' %d\n", ObjStr(element), i);*/
      /*Tcl_ListObjReplace(interp, varList, i, 1, 0, NULL);*/
    }
  }
  Tcl_SetObjResult(interp, okList);
  return TCL_OK;
}
/***********************************************************************
 * End Object Info Methods
 ***********************************************************************/

/***********************************************************************
 * Begin Class Info methods
 ***********************************************************************/

/*
classInfoMethod filterguard NsfClassInfoFilterguardMethod {
  {-argName "filter" -required 1}
  }
*/
static int
NsfClassInfoFilterguardMethod(Tcl_Interp *interp, NsfClass *class, const char *filter) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(filter != NULL);

  return (class->opt != NULL) ? GuardList(interp, class->opt->classFilters, filter) : TCL_OK;
}

/*
classInfoMethod filters NsfClassInfoFiltersMethod {
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "pattern"}
}
*/
static int
NsfClassInfoFiltersMethod(Tcl_Interp *interp, NsfClass *class,
                          int withGuards, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  return (class->opt != NULL) ? FilterInfo(interp, class->opt->classFilters, pattern, withGuards, 0) : TCL_OK;
}

/*
classInfoMethod forward NsfClassInfoForwardMethod {
  {-argName "-definition"}
  {-argName "name"}
}
*/
static int
NsfClassInfoForwardMethod(Tcl_Interp *interp, NsfClass *class,
                          int withDefinition, const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  return ListForward(interp, Tcl_Namespace_cmdTablePtr(class->nsPtr), pattern, withDefinition);
}

/*
classInfoMethod heritage NsfClassInfoHeritageMethod {
  {-argName "pattern"}
}
*/
static int
NsfClassInfoHeritageMethod(Tcl_Interp *interp, NsfClass *cl, const char *pattern) {
  NsfClasses *pl, *intrinsic, *checkList = NULL, *mixinClasses = NULL;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(cl != NULL);

  resultObj = Tcl_NewObj();
  intrinsic = PrecedenceOrder(cl);

  NsfClassListAddPerClassMixins(interp, cl, &mixinClasses, &checkList);
  for (pl = mixinClasses; pl; pl = pl->nextPtr) {
    if (NsfClassListFind(pl->nextPtr, pl->cl) == NULL &&
        NsfClassListFind(intrinsic, pl->cl) == NULL) {
      AppendMatchingElement(interp, resultObj, pl->cl->object.cmdName, pattern);
    }
  }

  if (intrinsic != NULL) {
    for (pl = intrinsic->nextPtr; pl; pl = pl->nextPtr) {
      AppendMatchingElement(interp, resultObj, pl->cl->object.cmdName, pattern);
    }
  }

  if (mixinClasses != NULL) {
    NsfClassListFree(mixinClasses);
  }
  if (checkList != NULL) {
    NsfClassListFree(checkList);
  }

  Tcl_SetObjResult(interp, resultObj);
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * InstancesFromClassList --
 *
 *      Collect all instances of the classes of the provided class list in the
 *      returned result object.
 *
 * Results:
 *      Tcl_Obj containing a list of instances or a single instance
 *
 * Side effects:
 *      Updated resultObj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *InstancesFromClassList(Tcl_Interp *interp, NsfClasses *subClasses,
                                       const char *pattern, NsfObject *matchObject)
  nonnull(1) nonnull(2) returns_nonnull;

static Tcl_Obj *
InstancesFromClassList(Tcl_Interp *interp, NsfClasses *subClasses,
                       const char *pattern, NsfObject *matchObject) {
  Tcl_Obj *resultObj = Tcl_NewObj();

  nonnull_assert(interp != NULL);
  nonnull_assert(subClasses != NULL);

  do {
    Tcl_HashTable *tablePtr = &subClasses->cl->instances;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search); hPtr;
         hPtr = Tcl_NextHashEntry(&search)) {
      NsfObject *inst = (NsfObject *) Tcl_GetHashKey(tablePtr, hPtr);

      if (matchObject != NULL && inst == matchObject) {
        Tcl_SetStringObj(resultObj, ObjStr(matchObject->cmdName), -1);
        return resultObj;
      }
      AppendMatchingElement(interp, resultObj, inst->cmdName, pattern);
    }
    subClasses = subClasses->nextPtr;
  } while (subClasses != NULL);

  return resultObj;
}

/*
classInfoMethod instances NsfClassInfoInstancesMethod {
  {-argName "-closure" -nrargs 0}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfClassInfoInstancesMethod(Tcl_Interp *interp, NsfClass *startCl,
                            int withClosure, const char *pattern, NsfObject *matchObject) {
  NsfClasses clElement, *subClasses;

  nonnull_assert(interp != NULL);
  nonnull_assert(startCl != NULL);

  if (withClosure != 0) {
    subClasses = TransitiveSubClasses(startCl);
  } else {
    subClasses = &clElement;
    clElement.cl = startCl;
    clElement.nextPtr = NULL;
  }

  Tcl_SetObjResult(interp, InstancesFromClassList(interp, subClasses, pattern, matchObject));

  if (withClosure != 0) {
    NsfClassListFree(subClasses);
  }

  return TCL_OK;
}

/*
classInfoMethod method NsfClassInfoMethodMethod {
  {-argName "infomethodsubcmd" -type "args|body|definition|exists|registrationhandle|definitionhandle|origin|parameter|syntax|type|precondition|postcondition|submethods|returns"}
  {-argName "name" -required 1 -type tclobj}
}
*/
static int
NsfClassInfoMethodMethod(Tcl_Interp *interp, NsfClass *class,
                         int subcmd, Tcl_Obj *methodNameObj) {

  return ListMethodResolve(interp, subcmd, NULL, NULL, class->nsPtr, &class->object, methodNameObj, 1);
}

/*
classInfoMethod methods NsfClassInfoMethodsMethod {
  {-argName "-callprotection" -type "all|public|protected|private" -default all}
  {-argName "-closure" -nrargs 0}
  {-argName "-type" -typeName "methodtype" -nrargs 1 -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-path" -nrargs 0}
  {-argName "-source" -nrargs 1 -type "all|application|system"}
  {-argName "pattern"}
}
*/
static int
NsfClassInfoMethodsMethod(Tcl_Interp *interp, NsfClass *class,
                          int withCallprotection,
                          int withClosure,
                          int withMethodtype,
                          int withPath,
                          int withSource,
                          const char *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  if (withClosure != 0) {
    NsfClasses *checkList = NULL, *mixinClasses = NULL;
    Tcl_HashTable dupsTable, *dups = &dupsTable;
    int result;

#if 0
    if (withCallprotection == CallprotectionNULL) {
      withCallprotection = CallprotectionPublicIdx;
    }
#endif
    if (withSource == SourceNULL) {
      withSource = SourceAllIdx;
    }

    Tcl_InitHashTable(dups, TCL_STRING_KEYS);
    /* guards are ignored */
    NsfClassListAddPerClassMixins(interp, class, &mixinClasses, &checkList);
    (void) ListMethodKeysClassList(interp, mixinClasses,
                                   withSource, pattern,
                                   AggregatedMethodType(withMethodtype), withCallprotection,
                                   withPath, dups, &class->object, 0);
    if (checkList != NULL) {
      NsfClassListFree(checkList);
    }
    if (mixinClasses != NULL) {
      NsfClassListFree(mixinClasses);
    }

    result = ListMethodKeysClassList(interp, PrecedenceOrder(class),
                                     withSource, pattern,
                                     AggregatedMethodType(withMethodtype), withCallprotection,
                                     withPath, dups, &class->object, 0);

    Tcl_DeleteHashTable(dups);
    return result;
  } else {
    if (withSource != 0) {
      return NsfPrintError(interp, "-source cannot be used without -closure\n");
    }
    return ListDefinedMethods(interp, &class->object, pattern, 0 /* per-object */,
                              AggregatedMethodType(withMethodtype), withCallprotection,
                              withPath);
  }
}

/*
classInfoMethod mixins NsfClassInfoMixinsMethod {
  {-argName "-closure" -nrargs 0 -type switch}
  {-argName "-guards" -nrargs 0 -type switch}
  {-argName "-heritage" -nrargs 0 -type switch}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfClassInfoMixinsMethod(Tcl_Interp *interp, NsfClass *class,
                         int withClosure, int withGuards, int withHeritage,
                         const char *patternString, NsfObject *patternObj) {
  NsfClassOpt *opt;
  Tcl_Obj *resultObj;
  int result = TCL_OK;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  opt = class->opt;
  Tcl_ResetResult(interp);
  resultObj = Tcl_GetObjResult(interp);

  if (withHeritage != 0) {
    NsfClasses *checkList = NULL, *mixinClasses = NULL, *clPtr;

    if (withGuards != 0) {
      return NsfPrintError(interp, "-guards cannot be used together with -heritage\n");
    }

    NsfClassListAddPerClassMixins(interp, class, &mixinClasses, &checkList);
    for (clPtr = mixinClasses; clPtr; clPtr = clPtr->nextPtr) {
      if (NsfClassListFind(clPtr->nextPtr, clPtr->cl)) {
        continue;
      }
      AppendMatchingElement(interp, resultObj, clPtr->cl->object.cmdName, patternString);
    }

    if (checkList != NULL) {
      NsfClassListFree(checkList);
    }
    if (mixinClasses != NULL) {
      NsfClassListFree(mixinClasses);
    }

  } else if (withClosure != 0) {
    Tcl_HashTable objTable, *commandTable = &objTable;
    int rc;

    MEM_COUNT_ALLOC("Tcl_InitHashTable", commandTable);
    Tcl_InitHashTable(commandTable, TCL_ONE_WORD_KEYS);
    rc = GetAllClassMixins(interp, commandTable, resultObj,
                           class, withGuards,
                           patternString, patternObj);
    if (patternObj && rc && !withGuards) {
      Tcl_SetObjResult(interp, (rc != 0) ? patternObj->cmdName : NsfGlobalObjs[NSF_EMPTY]);
    }
    Tcl_DeleteHashTable(commandTable);
    MEM_COUNT_FREE("Tcl_InitHashTable", commandTable);

  } else {
    result = (opt != NULL) ? MixinInfo(interp, opt->classMixins, patternString, withGuards, patternObj) : TCL_OK;
  }

  return result;
}

/*
classInfoMethod mixinguard NsfClassInfoMixinguardMethod {
  {-argName "mixin"  -required 1}
}
*/
static int
NsfClassInfoMixinguardMethod(Tcl_Interp *interp, NsfClass *class, const char *mixin) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);
  nonnull_assert(mixin != NULL);

  return (class->opt != NULL) ? GuardList(interp, class->opt->classMixins, mixin) : TCL_OK;
}

/*
classInfoMethod mixinof  NsfClassInfoMixinOfMethod {
  {-argName "-closure" -nrargs 0}
  {-argName "-scope" -required 0 -nrargs 1 -type "all|class|object"}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfClassInfoMixinOfMethod(Tcl_Interp *interp, NsfClass *class, int withClosure, int withScope,
                          const char *patternString, NsfObject *patternObj) {
  NsfClassOpt *opt;
  int perClass, perObject, rc = TCL_OK;
  Tcl_Obj *resultObj;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  opt = class->opt;
  Tcl_ResetResult(interp);
  resultObj = Tcl_GetObjResult(interp);

  if (withScope == ScopeNULL || withScope == ScopeAllIdx) {
    perClass = 1;
    perObject = 1;
  } else if (withScope == ScopeClassIdx) {
    perClass = 1;
    perObject = 0;
  } else {
    perClass = 0;
    perObject = 1;
  }

  if (opt != NULL && !withClosure) {
    if (perClass == 1 && opt->isClassMixinOf != NULL) {
      rc = AppendMatchingElementsFromCmdList(interp, opt->isClassMixinOf, resultObj,
                                             patternString, patternObj);
      if (rc && patternObj) {goto finished;}
    }
    if (perObject == 1 && opt->isObjectMixinOf) {
      rc = AppendMatchingElementsFromCmdList(interp, opt->isObjectMixinOf, resultObj,
                                             patternString, patternObj);
    }
  } else if (withClosure != 0) {
    Tcl_HashTable objTable, *commandTable = &objTable;

    MEM_COUNT_ALLOC("Tcl_InitHashTable", commandTable);
    Tcl_InitHashTable(commandTable, TCL_ONE_WORD_KEYS);
    if (perClass == 1) {
      rc = GetAllClassMixinsOf(interp, commandTable, resultObj,
                               class, 0, 1, patternString, patternObj);
      if (rc && patternObj) {goto finished;}
    }
    if (perObject == 1) {
      rc = GetAllObjectMixinsOf(interp, commandTable, resultObj,
                                class, 0, 1, patternString, patternObj);
    }
    Tcl_DeleteHashTable(commandTable);
    MEM_COUNT_FREE("Tcl_InitHashTable", commandTable);
  }

 finished:
  if (patternObj != NULL) {
    Tcl_SetObjResult(interp, (rc != 0) ? patternObj->cmdName : NsfGlobalObjs[NSF_EMPTY]);
  } else {
    Tcl_SetObjResult(interp, resultObj);
  }
  return TCL_OK;
}

/*
classInfoMethod slots NsfClassInfoSlotobjectsMethod {
  {-argName "-closure" -nrargs 0}
  {-argName "-source" -nrargs 1 -type "all|application|system"}
  {-argName "-type" -required 0 -nrargs 1 -type class}
  {-argName "pattern" -required 0}
}
*/
static int
NsfClassInfoSlotobjectsMethod(Tcl_Interp *interp, NsfClass *class,
                        int withClosure, int withSource, NsfClass *type,
                        const char *pattern) {
  NsfClasses *clPtr, *intrinsicClasses, *precedenceList = NULL;
  Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
  Tcl_HashTable slotTable;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  Tcl_ResetResult(interp);
  intrinsicClasses = PrecedenceOrder(class);

  if (withClosure != 0) {
    NsfClasses *checkList = NULL, *mixinClasses = NULL;
    /*
     * Compute the closure: first the transitive mixin-classes...
     */
    NsfClassListAddPerClassMixins(interp, class, &mixinClasses, &checkList);
    for (clPtr = mixinClasses; clPtr; clPtr = clPtr->nextPtr) {
      if (NsfClassListFind(clPtr->nextPtr, clPtr->cl) == NULL &&
          NsfClassListFind(intrinsicClasses, clPtr->cl) == NULL) {
        NsfClassListAdd(&precedenceList, clPtr->cl, NULL);
      }
    }
    /*
     * ... followed by the intrinsic classes.
     */
    NsfClassListAdd(&precedenceList, class, NULL);
    for (clPtr = intrinsicClasses->nextPtr; clPtr; clPtr = clPtr->nextPtr) {
      NsfClassListAdd(&precedenceList, clPtr->cl, NULL);
    }
    if (checkList != NULL) {
      NsfClassListFree(checkList);
    }
    if (mixinClasses != NULL) {
      NsfClassListFree(mixinClasses);
    }

  } else {
    NsfClassListAdd(&precedenceList, class, NULL);
  }
  /* NsfClassListPrint("precedence", precedenceList); */
  if (withSource == 0) {withSource = 1;}

  /*
   * Use a hash-table to eliminate potential duplicates.
   */
  Tcl_InitHashTable(&slotTable, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable", &slotTable);

  for (clPtr = precedenceList; clPtr; clPtr = clPtr->nextPtr) {
    if (MethodSourceMatches(withSource, clPtr->cl, NULL)) {
      AddSlotObjects(interp, &clPtr->cl->object, "::slot", &slotTable,
                     withSource, type, pattern, listObj);
    }
  }

  Tcl_DeleteHashTable(&slotTable);
  MEM_COUNT_FREE("Tcl_InitHashTable", &slotTable);

  if (precedenceList != NULL) {
    NsfClassListFree(precedenceList);
  }
  Tcl_SetObjResult(interp, listObj);

  return TCL_OK;
}


/*
classInfoMethod subclass NsfClassInfoSubclassMethod {
  {-argName "-closure" -nrargs 0 -type switch}
  {-argName "-dependent" -nrargs 0 -type switch}
  {-argName "pattern" -type objpattern}
}
*/
static int
NsfClassInfoSubclassMethod(Tcl_Interp *interp, NsfClass *class,
                           int withClosure, int withDependent,
                           const char *patternString, NsfObject *patternObj) {
  int rc = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  if (withClosure && withDependent) {
    return NsfPrintError(interp, "only -closure or -dependent can be specified, not both");
  }

  if (withClosure || withDependent) {
    NsfClasses *subClasses = (withClosure != 0) ? TransitiveSubClasses(class) : DependentSubClasses(class);

    if (subClasses != NULL) {
      rc = AppendMatchingElementsFromClasses(interp, subClasses, patternString, patternObj);
      NsfClassListFree(subClasses);
    }
  } else if (class->sub != NULL) {
    rc = AppendMatchingElementsFromClasses(interp, class->sub, patternString, patternObj);
  }

  if (patternObj != NULL) {
    Tcl_SetObjResult(interp, (rc != 0) ? patternObj->cmdName : NsfGlobalObjs[NSF_EMPTY]);
  }

  return TCL_OK;
}

/*
classInfoMethod superclass NsfClassInfoSuperclassMethod {
  {-argName "-closure" -nrargs 0}
  {-argName "pattern" -type tclobj}
}
*/
static int
NsfClassInfoSuperclassMethod(Tcl_Interp *interp, NsfClass *class, int withClosure, Tcl_Obj *pattern) {

  nonnull_assert(interp != NULL);
  nonnull_assert(class != NULL);

  return ListSuperClasses(interp, class, pattern, withClosure);
}

/***********************************************************************
 * End Class Info methods
 ***********************************************************************/

/*
 * Initialization and Exit handlers
 */

#ifdef DO_FULL_CLEANUP
/* delete global variables and procs */
static void DeleteProcsAndVars(Tcl_Interp *interp, Tcl_Namespace *nsPtr, int withKeepvars)
  nonnull(1) nonnull(2);

static void
DeleteProcsAndVars(Tcl_Interp *interp, Tcl_Namespace *nsPtr, int withKeepvars) {
  Tcl_HashTable *varTablePtr, *cmdTablePtr, *childTablePtr;
  Tcl_HashSearch search;
  Tcl_Command cmd;
  Var *varPtr;
  register Tcl_HashEntry *entryPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(nsPtr != NULL);

  /* fprintf(stderr, "DeleteProcsAndVars in %s\n", nsPtr->fullName); */

  varTablePtr = (Tcl_HashTable *)Tcl_Namespace_varTablePtr(nsPtr);
  cmdTablePtr = Tcl_Namespace_cmdTablePtr(nsPtr);
  childTablePtr = Tcl_Namespace_childTablePtr(nsPtr);

  /*
   * Deleting the procs and vars in the child namespaces does not seem to be
   * necessary, but we do it anyway.
   */
  for (entryPtr = Tcl_FirstHashEntry(childTablePtr, &search); entryPtr;
       entryPtr = Tcl_NextHashEntry(&search)) {
    Tcl_Namespace *childNsPtr = (Tcl_Namespace *) Tcl_GetHashValue(entryPtr);
    DeleteProcsAndVars(interp, childNsPtr, withKeepvars);
  }

  if (withKeepvars == 0) {
    for (entryPtr = Tcl_FirstHashEntry(varTablePtr, &search); entryPtr;
         entryPtr = Tcl_NextHashEntry(&search)) {
      Tcl_Obj *nameObj;
      GetVarAndNameFromHash(entryPtr, &varPtr, &nameObj);
      if (!TclIsVarUndefined(varPtr) || TclIsVarNamespaceVar(varPtr)) {
        /* fprintf(stderr, "unsetting var %s\n", ObjStr(nameObj));*/
        Tcl_UnsetVar2(interp, ObjStr(nameObj), (char *)NULL, TCL_GLOBAL_ONLY);
      }
    }
  }

  for (entryPtr = Tcl_FirstHashEntry(cmdTablePtr, &search); entryPtr;
       entryPtr = Tcl_NextHashEntry(&search)) {
    cmd = (Tcl_Command)Tcl_GetHashValue(entryPtr);

    if (Tcl_Command_objProc(cmd) == RUNTIME_STATE(interp)->objInterpProc) {
      /*fprintf(stderr, "cmdname = %s cmd %p proc %p objProc %p %d\n",
        Tcl_GetHashKey(cmdTablePtr, entryPtr), cmd, Tcl_Command_proc(cmd), Tcl_Command_objProc(cmd),
        Tcl_Command_proc(cmd)==RUNTIME_STATE(interp)->objInterpProc);*/

      Tcl_DeleteCommandFromToken(interp, cmd);
    }
  }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * FinalObjectDeletion --
 *
 *      The method is to be called, when an object is finally deleted,
 *      typically during the final cleanup. It tests as well the actication
 *      count of the object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Deletion of the objects.
 *
 *----------------------------------------------------------------------
 */
static void
FinalObjectDeletion(Tcl_Interp *interp, NsfObject *object) {

  nonnull_assert(interp != NULL);
  nonnull_assert(object != NULL);

  /*
   * If a call to exit happens from a higher stack frame, the object
   * refCount might not be decremented correctly. If we are in the
   * physical destroy round, we can set the counter to an appropriate
   * value to ensure deletion.
   */
#if defined(NSF_DEVELOPMENT)
  if (unlikely(object->refCount != 1)) {
    if (object->refCount > 1) {
      NsfLog(interp, NSF_LOG_WARN,  "RefCount for obj %p %d (name %s) > 1",
             (void *)object, object->refCount, ObjectName(object));
    } else {
      NsfLog(interp, NSF_LOG_WARN,  "Refcount for obj %p %d > 1",
             (void *)object, object->refCount);
    }
    /*object->refCount = 1;*/
  }
#endif

#if !defined(NDEBUG)
  if (unlikely(object->activationCount != 0)) {
    fprintf(stderr, "FinalObjectDeletion obj %p activationcount %d\n",
            (void *)object, object->activationCount);
  }
#endif
  assert(object->activationCount == 0);

  if (likely(object->id != NULL)) {
    /*fprintf(stderr, "  ... cmd dealloc %p final delete refCount %d\n",
      object->id, Tcl_Command_refCount(object->id));*/

    if (NSF_DTRACE_OBJECT_FREE_ENABLED()) {
      NSF_DTRACE_OBJECT_FREE(ObjectName(object), ClassName(object->cl));
    }

    Tcl_DeleteCommandFromToken(interp, object->id);
  }
}

#ifdef DO_CLEANUP
/*
 *----------------------------------------------------------------------
 *
 * DeleteNsfProcs --
 *
 *      Delete all nsfprocs in the namespaces rooted by the second
 *      argument. If the provided nsPtr is NULL, the global namespace is used
 *      as root of the namespace tree. The function is necessary to trigger
 *      the freeing of the parameter definitions.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Deletion of nsfprocs.
 *
 *----------------------------------------------------------------------
 */
static void DeleteNsfProcs(Tcl_Interp *interp, Tcl_Namespace *nsPtr) nonnull(1);

static void
DeleteNsfProcs(Tcl_Interp *interp, Tcl_Namespace *nsPtr) {
  Tcl_HashTable *cmdTablePtr, *childTablePtr;
  register Tcl_HashEntry *entryPtr;
  Tcl_HashSearch search;

  nonnull_assert(interp != NULL);

  if (nsPtr == NULL) {
    nsPtr = Tcl_GetGlobalNamespace(interp);
  }

  nonnull_assert(nsPtr != NULL);
  /*fprintf(stderr, "### DeleteNsfProcs current namespace '%s'\n", (nsPtr != NULL) ? nsPtr->fullName : "NULL");*/

  cmdTablePtr = Tcl_Namespace_cmdTablePtr(nsPtr);
  childTablePtr = Tcl_Namespace_childTablePtr(nsPtr);

  for (entryPtr = Tcl_FirstHashEntry(cmdTablePtr, &search); entryPtr;
       entryPtr = Tcl_NextHashEntry(&search)) {
    Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(entryPtr);

    if (Tcl_Command_objProc(cmd) == NsfProcStub) {
      /*fprintf(stderr, "cmdname = %s cmd %p\n",
        Tcl_GetHashKey(cmdTablePtr, entryPtr), cmd);*/
      Tcl_DeleteCommandFromToken(interp, cmd);
    }
  }
  for (entryPtr = Tcl_FirstHashEntry(childTablePtr, &search); entryPtr;
       entryPtr = Tcl_NextHashEntry(&search)) {
    Tcl_Namespace *childNsPtr = (Tcl_Namespace *) Tcl_GetHashValue(entryPtr);

    DeleteNsfProcs(interp, childNsPtr);
  }
}

/*
 *----------------------------------------------------------------------
 *
 * ClassHasSubclasses --
 *
 *      Check, whether the given class has subclasses.
 *
 * Results:
 *      boolean
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int ClassHasSubclasses(NsfClass *cl) nonnull(1);

static int
ClassHasSubclasses(NsfClass *cl) {

  nonnull_assert(cl != NULL);

  return (cl->sub != NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ClassHasInstances --
 *
 *      Check, whether the given class has instances.
 *
 * Results:
 *      boolean
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int ClassHasInstances(NsfClass *cl) nonnull(1);

static int
ClassHasInstances(NsfClass *cl) {
  Tcl_HashSearch hSrch;

  nonnull_assert(cl != NULL);

  return (Tcl_FirstHashEntry(&cl->instances, &hSrch) != NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ObjectHasChildren --
 *
 *      Check, whether the given object has children
 *
 * Results:
 *      boolean
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int ObjectHasChildren(NsfObject *object) nonnull(1);

static int
ObjectHasChildren(NsfObject *object) {
  Tcl_Namespace *ns;
  int result = 0;

  nonnull_assert(object != NULL);

  ns = object->nsPtr;
  if (ns != NULL) {
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSrch;
    Tcl_HashTable *cmdTablePtr = Tcl_Namespace_cmdTablePtr(ns);

    for (hPtr = Tcl_FirstHashEntry(cmdTablePtr, &hSrch); hPtr;
         hPtr = Tcl_NextHashEntry(&hSrch)) {
      Tcl_Command cmd = Tcl_GetHashValue(hPtr);
      NsfObject *childObject = NsfGetObjectFromCmdPtr(cmd);

      if (childObject != NULL) {
        result = 1;
        break;
      }
    }
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeAllNsfObjectsAndClasses --
 *
 *      Destroy and free all objects and classes defined int the interp.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Freeing memory.
 *
 *----------------------------------------------------------------------
 */
static void FreeAllNsfObjectsAndClasses(Tcl_Interp *interp, NsfCmdList **instances)
  nonnull(1) nonnull(2);

static void
FreeAllNsfObjectsAndClasses(Tcl_Interp *interp, NsfCmdList **instances) {
  NsfCmdList *entry, *lastEntry;
  int deleted = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(instances != NULL);

  /*fprintf(stderr, "FreeAllNsfObjectsAndClasses in %p\n", interp);*/

  RUNTIME_STATE(interp)->exitHandlerDestroyRound = NSF_EXITHANDLER_ON_PHYSICAL_DESTROY;

  /*
   * First delete all child commands of all objects, which are not
   * objects themselves. This will for example delete namespace
   * imported commands and objects and will resolve potential loops in
   * the dependency graph. The result is a plain object/class tree.
   */

  for (entry = *instances; entry; entry = entry->nextPtr) {
    NsfObject *object = (NsfObject *)entry->clorobj;

    /* delete per-object methods */
    if (object != NULL && object->nsPtr != NULL) {
      Tcl_HashEntry *hPtr;
      Tcl_HashSearch hSrch;

      for (hPtr = Tcl_FirstHashEntry(Tcl_Namespace_cmdTablePtr(object->nsPtr), &hSrch); hPtr;
           hPtr = Tcl_NextHashEntry(&hSrch)) {
        Tcl_Command cmd = Tcl_GetHashValue(hPtr);

        if (cmd != NULL) {
          if (CmdIsNsfObject(cmd)) {
            AliasDeleteObjectReference(interp, cmd);
            continue;
          }
          Tcl_DeleteCommandFromToken(interp, cmd);
          deleted ++;
        }
      }
    }

    /*
     * Delete class methods; these methods might have aliases (dependencies) to
     * objects, which will resolved this way.
     */
    if (object != NULL && NsfObjectIsClass(object)) {
      Tcl_HashEntry *hPtr;
      Tcl_HashSearch hSrch;

      for (hPtr = Tcl_FirstHashEntry(Tcl_Namespace_cmdTablePtr(((NsfClass *)object)->nsPtr),
                                      &hSrch); hPtr;
           hPtr = Tcl_NextHashEntry(&hSrch)) {
        Tcl_Command cmd = Tcl_GetHashValue(hPtr);

        if (cmd && CmdIsNsfObject(cmd)) {
          AliasDeleteObjectReference(interp, cmd);
          continue;
        }
      }
    }
  }

  /*fprintf(stderr, "deleted %d cmds\n", deleted);*/

  /*
   * Finally delete the object/class tree in a bottom up manner,
   * deleting all objects without dependencies first. Finally, only
   * the root classes of the object system will remain, which are
   * deleted separately.
   */

  while (1) {
    /*
     * Delete all plain objects without dependencies.
     */
    deleted = 0;
    for (entry = *instances, lastEntry = NULL;
         entry;
         lastEntry = entry, entry = entry->nextPtr) {
      NsfObject *object = (NsfObject *)entry->clorobj;

      /*
       * The list of the instances should contain only alive objects, without
       * duplicates. We would recognize duplicates since a deletion of one
       * object would trigger the CMD_IS_DELETED flag of the cmdPtr of the
       * duplicate.
       */
      assert((Tcl_Command_flags(entry->cmdPtr) & CMD_IS_DELETED) == 0);

      if (object != NULL && !NsfObjectIsClass(object) && !ObjectHasChildren(object)) {
        /*fprintf(stderr, "check %p obj->flags %.6x cmd %p deleted %d\n",
                object, object->flags, entry->cmdPtr,
                Tcl_Command_flags(entry->cmdPtr) & CMD_IS_DELETED); */
        assert(object->id != NULL);
        /*fprintf(stderr, "  ... delete object %s %p, class=%s id %p ns %p\n",
          ObjectName(object), object,
          ClassName(object->cl), object->id, object->nsPtr);*/

        FreeUnsetTraceVariable(interp, object);
        FinalObjectDeletion(interp, object);

        if (entry == *instances) {
          *instances = entry->nextPtr;
          CmdListDeleteCmdListEntry(entry, NULL);
          entry = *instances;
        } else {
          lastEntry->nextPtr = entry->nextPtr;
          CmdListDeleteCmdListEntry(entry, NULL);
          entry = lastEntry;
        }
        assert(entry != NULL);

        deleted++;
      }
    }
    /*fprintf(stderr, "deleted %d Objects without dependencies\n", deleted);*/

    if (deleted > 0) {
      continue;
    }

    /*
     * Delete all classes without dependencies.
     */
    for (entry = *instances, lastEntry = NULL;
         entry;
         lastEntry = entry, entry = entry->nextPtr) {
      NsfClass *cl = entry->clorobj;

      assert(cl != NULL);

      if (!NsfObjectIsClass(&cl->object)) {
        continue;
      }

      /*fprintf(stderr, "cl key = %s %p\n", ClassName(cl), cl); */
      if (!ObjectHasChildren((NsfObject *)cl)
          && !ClassHasInstances(cl)
          && !ClassHasSubclasses(cl)
          && !IsBaseClass(&cl->object)
          ) {
        /*fprintf(stderr, "  ... delete class %s %p\n", ClassName(cl), cl); */
        assert(cl->object.id);

        FreeUnsetTraceVariable(interp, &cl->object);
        FinalObjectDeletion(interp, &cl->object);

        if (entry == *instances) {
          *instances = entry->nextPtr;
          /*fprintf(stderr, "... delete first entry %p\n", entry);*/
          CmdListDeleteCmdListEntry(entry, NULL);
          entry = *instances;
        } else {
          /*fprintf(stderr, "... delete entry %p\n", entry);*/
          lastEntry->nextPtr = entry->nextPtr;
          CmdListDeleteCmdListEntry(entry, NULL);
          entry = lastEntry;
        }

        deleted++;
      }
    }

    /*fprintf(stderr, "deleted %d Classes\n", deleted);*/

    if (deleted == 0) {
      int reclassed = 0;

      /*
       * Final check. If there are no cyclical dependencies, we should have
       * now just the the base classes left. If this is not the case, reclass
       * the remaining objects to their base classes, and set the superClasses
       * to the most general superclass.
       */
      for (entry = *instances;
           entry;
           entry = entry->nextPtr) {
        NsfObject *object = (NsfObject *)entry->clorobj;
        NsfClass *baseClass;
        NsfObjectSystem *osPtr;

        if (NsfObjectIsClass(object) && IsBaseClass(object)) {
          continue;
        }

        osPtr = GetObjectSystem(object);

        /*
         * For classes, check the superclass hierarchy.
         */
        if (NsfObjectIsClass(object)) {
          NsfClass *cl = (NsfClass *)object;
          NsfClasses *sc;

          for (sc = cl->super; sc; sc = sc->nextPtr) {
            if (sc->cl != osPtr->rootClass) {
              Tcl_Obj *objectName = osPtr->rootClass->object.cmdName;
              SuperclassAdd(interp, cl, 1, &objectName, objectName);
              reclassed ++;
              break;
            }
          }
        }

        /*
         * In all cases, straigthen the class to the base case.
         */
        baseClass = NsfObjectIsClass(object) ? osPtr->rootMetaClass : osPtr->rootClass;
        if (object->cl != baseClass) {
          ChangeClass(interp, object, baseClass);
          reclassed ++;
        }
      }
      /*fprintf(stderr, "We have reclassed %d objects\n", reclassed);*/

      if (reclassed == 0) {
        break;
      }
    }
  }
}

#endif /* DO_CLEANUP */

/*
 *  Exit Handler
 */
static void ExitHandler(ClientData clientData) nonnull(1);

static void
ExitHandler(ClientData clientData) {
  Tcl_Interp *interp = (Tcl_Interp *)clientData;
  int i, flags;
  NsfRuntimeState *rst = RUNTIME_STATE(interp);

  nonnull_assert(clientData != NULL);

  /*fprintf(stderr, "ExitHandler\n");*/

  /*
   * Don't use exit handler, if the interpreter is already destroyed.
   * Call to exit handler comes after freeing namespaces, commands, etc.
   * e.g. TK calls Tcl_DeleteInterp directly, if Window is killed.
   */

  /*
   * Ahem ...
   *
   * Since we *must* be sure that our destroy methods will run
   * we must *cheat* (I mean CHEAT) here: we flip the interp
   * flag, saying, "hey boy, you're not deleted any more".
   * After our handlers are done, we restore the old state...
   * All this is needed so we can do an eval in the interp which
   * is potentially marked for delete when we start working here.
   *
   * I know, I know, this is not really elegant. But...  I'd need a
   * standard way of invoking some code at interpreter delete time
   * but JUST BEFORE the actual deletion process starts. Sadly,
   * there is no such hook in Tcl as of Tcl8.4.*, that I know of.
   *
   * So, for the rest of procedure, assume the interp is alive !
   */
  flags = Tcl_Interp_flags(interp);
  Tcl_Interp_flags(interp) &= ~DELETED;

  CallStackPopAll(interp);

  if (rst->exitHandlerDestroyRound == NSF_EXITHANDLER_OFF) {
    NsfFinalizeCmd(interp, 0);
  }

  /* Must be before freeing of NsfGlobalObjs */
  NsfShadowTclCommands(interp, SHADOW_UNLOAD);

  MEM_COUNT_FREE("Tcl_InitHashTable", &rst->activeFilterTablePtr);
  Tcl_DeleteHashTable(&rst->activeFilterTablePtr);

  /* free global objects */
  for (i = 0; i < nr_elements(NsfGlobalStrings); i++) {
    DECR_REF_COUNT(NsfGlobalObjs[i]);
  }
  NsfStringIncrFree(&rst->iss);

  /*
   * Free all data in the pointer converter.
   */
  Nsf_PointerExit(interp);

#if defined(NSF_PROFILE)
  NsfProfileFree(interp);
#endif

  FREE(Tcl_Obj**, NsfGlobalObjs);

#if defined(TCL_MEM_DEBUG)
  TclDumpMemoryInfo((ClientData) stderr, 0);
  Tcl_DumpActiveMemory("./nsfActiveMem");
  /* Tcl_Eval(interp, "puts {checkmem to checkmemFile};
     checkmem checkmemFile"); */
#endif

  /*
   * Free runtime state.
   */
  ckfree((char *) RUNTIME_STATE(interp));
#if USE_ASSOC_DATA
  Tcl_DeleteAssocData(interp, "NsfRuntimeState");
#else
  Tcl_Interp_globalNsPtr(interp)->clientData = NULL;
#endif

#if defined(NSF_MEM_COUNT) && !defined(PRE86)
  /*
   * When raising an error, the Tcl_Objs on the error stack and in the
   * inner context are refCount-incremented. When Tcl exits, it does normally
   * not perform the according decrementing. We perform here a manual
   * decrementing and reset these lists.
   */
  {
    Interp *iPtr = (Interp *) interp;

    if (iPtr->innerContext != NULL) {
      Tcl_DecrRefCount(iPtr->errorStack);
      iPtr->errorStack = Tcl_NewListObj(0, NULL);
      Tcl_IncrRefCount(iPtr->errorStack);
      Tcl_DecrRefCount(iPtr->innerContext);
      iPtr->innerContext = Tcl_NewListObj(0, NULL);
      Tcl_IncrRefCount(iPtr->innerContext);
    }
  }
#endif

  Tcl_Interp_flags(interp) = flags;
  Tcl_Release(interp);

  MEM_COUNT_RELEASE();
}


#if defined(TCL_THREADS)
/*
 * Gets activated at thread-exit
 */
static void Nsf_ThreadExitProc(ClientData clientData) nonnull(1);

static void
Nsf_ThreadExitProc(ClientData clientData) {
  void Nsf_ExitProc(ClientData clientData);

  nonnull_assert(clientData != NULL);

  /*fprintf(stderr, "+++ Nsf_ThreadExitProc\n");*/

  Tcl_DeleteExitHandler(Nsf_ExitProc, clientData);
  ExitHandler(clientData);
}
#endif

/*
 * Gets activated at application-exit
 */
void Nsf_ExitProc(ClientData clientData) nonnull(1);

void
Nsf_ExitProc(ClientData clientData) {

  nonnull_assert(clientData != NULL);

  /*fprintf(stderr, "+++ Nsf_ExitProc\n");*/
#if defined(TCL_THREADS)
  Tcl_DeleteThreadExitHandler(Nsf_ThreadExitProc, clientData);
#endif
  ExitHandler(clientData);
}


/*
 * Registers thread/application exit handlers.
 */
static void RegisterExitHandlers(ClientData clientData) nonnull(1);

static void
RegisterExitHandlers(ClientData clientData) {

  nonnull_assert(clientData != NULL);

  Tcl_Preserve(clientData);
#if defined(TCL_THREADS)
  Tcl_CreateThreadExitHandler(Nsf_ThreadExitProc, clientData);
#endif
  Tcl_CreateExitHandler(Nsf_ExitProc, clientData);
}

/*
 * Tcl extension initialization routine
 */

#if 0
#include <google/profiler.h>
#endif

int Nsf_Init(Tcl_Interp *interp) nonnull(1);

int
Nsf_Init(Tcl_Interp *interp) {
  static NsfMutex initMutex = 0;
  ClientData runtimeState;
  NsfRuntimeState *rst;
  int result, i;
#ifdef NSF_BYTECODE
  /*NsfCompEnv *interpstructions = NsfGetCompEnv();*/
#endif
#ifdef USE_TCL_STUBS
  static int stubsInitialized = 0;
#endif

  nonnull_assert(interp != NULL);

#if 0
  ProfilerStart("profiler");
#endif

#ifdef USE_TCL_STUBS
  /*
   * Since the stub-tables are initialized globally (not per interp), we want
   * to initialize these only once.  The read operation on "stubsInitialized"
   * is a potentially dirty read. However, we can't use a mutex lock around
   * this, since Tcl_MutexLock() requires (at least on some platforms)
   * initialized stub-tables. The dirty read of stubsInitialized is not so
   * invasive as the dirty reads caused by overwriting the stub tables.
   *
   * NsfMutexLock(&stubFlagMutex);
   * ...
   * NsfMutexUnlock(&stubFlagMutex);
   */

  if (stubsInitialized == 0) {
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) {
      return TCL_ERROR;
    }
    if (Tcl_TomMath_InitStubs(interp, "8.5") == NULL) {
      return TCL_ERROR;
    }
    stubsInitialized = 1;
  }
#endif

#if defined(TCL_MEM_DEBUG)
  TclDumpMemoryInfo((ClientData) stderr, 0);
#endif

  /*
   * Runtime State stored in the client data of the Interp's global namespace
   * in order to avoid global state information. All fields are per default
   * set to zero.
   */
  runtimeState = ckalloc(sizeof(NsfRuntimeState));
  memset(runtimeState, 0, sizeof(NsfRuntimeState));

#if USE_ASSOC_DATA
  Tcl_SetAssocData(interp, "NsfRuntimeState", NULL, runtimeState);
#else
  Tcl_Interp_globalNsPtr(interp)->clientData = runtimeState;
#endif

  /*
   * If MEM_COUNT is activated, the tables have to be initialized before the
   * first call to the MEM_COUNT macros (including e.g. INCR_REF_COUNT), but
   * it requires that the runtimeState is already associated with the interp.
   */
  MEM_COUNT_INIT();

  /*
   * Init global variables for Tcl_Obj types.
   */
  NsfMutexLock(&initMutex);
  Nsf_OT_byteCodeType = Tcl_GetObjType("bytecode");
  assert(Nsf_OT_byteCodeType != NULL);

  Nsf_OT_tclCmdNameType = Tcl_GetObjType("cmdName");
  assert(Nsf_OT_tclCmdNameType != NULL);

  Nsf_OT_listType = Tcl_GetObjType("list");
  assert(Nsf_OT_listType != NULL);

  Nsf_OT_intType = Tcl_GetObjType("int");
  assert(Nsf_OT_intType != NULL);

  Nsf_OT_doubleType = Tcl_GetObjType("double");
  assert(Nsf_OT_doubleType != NULL);
  NsfMutexUnlock(&initMutex);

  /*
   * Initialize the pointer converter, the enumeration types and cmd
   * definitions tables and load it with the generated information for
   * introspection.
   */
  Nsf_PointerInit(interp);

  Nsf_EnumerationTypeInit(interp);
  result = Nsf_EnumerationTypeRegister(interp, enumeratorConverterEntries);
  if (unlikely(result != TCL_OK)) {
    return result;
  }

  Nsf_CmdDefinitionInit(interp);
  Nsf_CmdDefinitionRegister(interp, method_definitions);

  /*
    fprintf(stderr, "SIZES: obj=%d, tcl_obj=%d, DString=%d, class=%d, namespace=%d, command=%d, HashTable=%d\n",
    sizeof(NsfObject), sizeof(Tcl_Obj), sizeof(Tcl_DString), sizeof(NsfClass),
    sizeof(Namespace), sizeof(Command), sizeof(Tcl_HashTable));
  */

#if defined(NSF_PROFILE)
  NsfProfileInit(interp);
#endif
  rst = RUNTIME_STATE(interp);
  rst->doFilters = 1;
  rst->doCheckResults = 1;
  rst->doCheckArguments = NSF_ARGPARSE_CHECK;

#if defined(NSF_STACKCHECK)
  { int someVar;
    /*
     * Note that Nsf_Init() is called typically via a package require, which
     * is therefore not really the bottom of the stack, but just a first
     * approximization.
     */
    rst->bottomOfStack = &someVar;
    rst->maxStack = rst->bottomOfStack;
  }
#endif

  /*
   * Check, if the namespace exists, otherwise create it.
   */
  rst->NsfNS = Tcl_FindNamespace(interp, "::nsf", NULL, TCL_GLOBAL_ONLY);
  if (rst->NsfNS == NULL) {
    rst->NsfNS = Tcl_CreateNamespace(interp, "::nsf", NULL,
                                     (Tcl_NamespaceDeleteProc *)NULL);
  }
  MEM_COUNT_ALLOC("TclNamespace", rst->NsfNS);

  /*
   * Init an empty, faked proc structure in the RUNTIME state.
   */
  rst->fakeProc.iPtr = (Interp *)interp;
  rst->fakeProc.refCount = 1;
  rst->fakeProc.cmdPtr = NULL;
  rst->fakeProc.bodyPtr = NULL;
  rst->fakeProc.numArgs = 0;
  rst->fakeProc.numCompiledLocals = 0;
  rst->fakeProc.firstLocalPtr = NULL;
  rst->fakeProc.lastLocalPtr = NULL;

  /*
   * NsfClasses in separate Namespace / Objects
   */
  rst->NsfClassesNS =
    Tcl_CreateNamespace(interp, "::nsf::classes", NULL,
                        (Tcl_NamespaceDeleteProc *)NULL);
  MEM_COUNT_ALLOC("TclNamespace", rst->NsfClassesNS);

  /*
   * Cache interpreters proc interpretation functions
   */
  rst->objInterpProc = TclGetObjInterpProc();
  rst->exitHandlerDestroyRound = NSF_EXITHANDLER_OFF;

  RegisterExitHandlers(interp);
  NsfStringIncrInit(&RUNTIME_STATE(interp)->iss);
  /*
   * initialize global Tcl_Obj
   */
  NsfGlobalObjs = NEW_ARRAY(Tcl_Obj*, nr_elements(NsfGlobalStrings));

  for (i = 0; i < nr_elements(NsfGlobalStrings); i++) {
    NsfGlobalObjs[i] = Tcl_NewStringObj(NsfGlobalStrings[i], -1);
    INCR_REF_COUNT(NsfGlobalObjs[i]);
  }

  Tcl_InitHashTable(&rst->activeFilterTablePtr, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable", &rst->activeFilterTablePtr);

  /*
   * Create namespaces for the different command types.
   */
  Tcl_CreateNamespace(interp, "::nsf::cmd", 0, (Tcl_NamespaceDeleteProc *)NULL);
  for (i = 0; i < nr_elements(method_command_namespace_names); i++) {
    Tcl_CreateNamespace(interp, method_command_namespace_names[i], 0, (Tcl_NamespaceDeleteProc *)NULL);
  }

  /*
   * Create all method commands (will use the namespaces above).
   */
  for (i = 0; i < nr_elements(method_definitions)-1; i++) {
    Tcl_CreateObjCommand(interp, method_definitions[i].methodName, method_definitions[i].proc, 0, 0);
  }

  /*
   * Create Shadowed Tcl cmds:
   */
  result = NsfShadowTclCommands(interp, SHADOW_LOAD);
  if (unlikely(result != TCL_OK)) {
    return result;
  }
  /*
   * Create new Tcl cmds:
   */
#ifdef NSF_BYTECODE
  instructions[INST_NEXT].cmdPtr = (Command *)
#endif
    Tcl_CreateObjCommand(interp, "::nsf::xotclnext", NsfNextObjCmd, 0, 0);
#ifdef NSF_BYTECODE
  instructions[INST_SELF].cmdPtr =
    (Command *)Tcl_FindCommand(interp, "::nsf::current", NULL, TCL_GLOBAL_ONLY);
#endif
  /*Tcl_CreateObjCommand(interp, "::nsf::K", NsfKObjCmd, 0, 0);*/

#ifdef NSF_BYTECODE
  NsfBytecodeInit();
#endif
  NsfReportVars(interp);

  Tcl_AddInterpResolvers(interp,"nsf",
                         (Tcl_ResolveCmdProc *)InterpColonCmdResolver,
                         InterpColonVarResolver,
                         (Tcl_ResolveCompiledVarProc *)InterpCompiledColonVarResolver);
  rst->colonCmd = Tcl_FindCommand(interp, "::nsf::colon", NULL, TCL_GLOBAL_ONLY);

  /*
   *  Tcl occasionally resolves a proc's cmd structure (e.g., in
   *  [info frame /number/] or TclInfoFrame()) without
   *  verification. However, NSF non-proc frames, in particular
   *  initcmd blocks, point to the fakeProc structure which does not
   *  contain a initialized Command pointer. For now, we default to
   *  an internal command. However, we might have to revisit this decision
   *  as non-proc frames (e.g., initcmds) report a "proc" entry
   *  for c-based functions with a proc scope, such as "::nsf::colon"),
   *  which can lead to confusions. "proc" does not mean "tcp proc",
   *  but an entry with a proc frame for local vars.
  */
  rst->fakeProc.cmdPtr = (Command *)RUNTIME_STATE(interp)->colonCmd;

  {
    /*
     * The file "predefined.h" contains some methods and library procs
     * implemented in Tcl - they could go in a nsf.tcl file, but
     * they're embedded here with Tcl_Eval to avoid the need to
     * carry around a separate file at runtime.
     */

#include "predefined.h"

    /* fprintf(stderr, "predefined=<<%s>>\n", cmd);*/
    if (Tcl_Eval(interp, cmd) != TCL_OK) {
      static char cmd[] =
        "puts stderr \"Error in predefined code\n\
         $::errorInfo\"";
      Tcl_EvalEx(interp, cmd, -1, 0);
      return TCL_ERROR;
    }
  }

#ifndef AOL_SERVER
  /* the AOL server uses a different package loading mechanism */
# ifdef COMPILE_NSF_STUBS
  Tcl_PkgProvideEx(interp, "nsf", PACKAGE_VERSION, &nsfStubs);
# else
  Tcl_PkgProvide(interp, "nsf", PACKAGE_VERSION);
# endif
#endif

  /*
   * Obtain type for parsed var name.
   */
  if (Nsf_OT_parsedVarNameType == NULL) {
    Tcl_Obj *varNameObj = Tcl_NewStringObj("::nsf::version", -1);
    Var *arrayPtr;

    INCR_REF_COUNT(varNameObj);
    TclObjLookupVar(interp, varNameObj, NULL, 0, "access",
                    /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
    Nsf_OT_parsedVarNameType = varNameObj->typePtr;
    assert(Nsf_OT_parsedVarNameType != NULL);
    DECR_REF_COUNT(varNameObj);
  }

#if !defined(TCL_THREADS)
  if ((Tcl_GetVar2(interp, "tcl_platform", "threaded", TCL_GLOBAL_ONLY) != NULL)) {
    /* a non threaded version of nsf is loaded into a threaded environment */
    fprintf(stderr, "\n A non threaded version of the Next Scripting Framework "
            "is loaded into threaded environment.\n"
            "Please reconfigure nsf with --enable-threads!\n\n\n");
  }
#endif

  Tcl_ResetResult(interp);
  Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);

  return TCL_OK;
}


EXTERN int Nsf_SafeInit(Tcl_Interp *interp) nonnull(1);

EXTERN int
Nsf_SafeInit(Tcl_Interp *interp) {

  nonnull_assert(interp != NULL);

  /*** dummy for now **/
  return Nsf_Init(interp);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 78
 * indent-tabs-mode: nil
 * End:
 */
