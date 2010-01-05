/*
 * xotclDecls.h --
 *
 *	Declarations of functions in the platform independent public XOTcl API.
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 * See the file "tcl-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _XOTCLDECLS
#define _XOTCLDECLS

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the xotcl.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#ifndef Xotcl_Init_TCL_DECLARED
#define Xotcl_Init_TCL_DECLARED
/* 0 */
EXTERN int		Xotcl_Init (Tcl_Interp * interp);
#endif
/* Slot 1 is reserved */
#ifndef XOTclIsClass_TCL_DECLARED
#define XOTclIsClass_TCL_DECLARED
/* 2 */
EXTERN struct XOTcl_Class * XOTclIsClass (Tcl_Interp * interp, ClientData cd);
#endif
/* Slot 3 is reserved */
#ifndef XOTclGetObject_TCL_DECLARED
#define XOTclGetObject_TCL_DECLARED
/* 4 */
EXTERN struct XOTcl_Object * XOTclGetObject (Tcl_Interp * interp, 
				char * name);
#endif
#ifndef XOTclGetClass_TCL_DECLARED
#define XOTclGetClass_TCL_DECLARED
/* 5 */
EXTERN struct XOTcl_Class * XOTclGetClass (Tcl_Interp * interp, char * name);
#endif
#ifndef XOTclCreateObject_TCL_DECLARED
#define XOTclCreateObject_TCL_DECLARED
/* 6 */
EXTERN int		XOTclCreateObject (Tcl_Interp * interp, 
				Tcl_Obj * name, struct XOTcl_Class * cl);
#endif
/* Slot 7 is reserved */
#ifndef XOTclCreateClass_TCL_DECLARED
#define XOTclCreateClass_TCL_DECLARED
/* 8 */
EXTERN int		XOTclCreateClass (Tcl_Interp * interp, 
				Tcl_Obj * name, struct XOTcl_Class * cl);
#endif
#ifndef XOTclDeleteObject_TCL_DECLARED
#define XOTclDeleteObject_TCL_DECLARED
/* 9 */
EXTERN int		XOTclDeleteObject (Tcl_Interp * interp, 
				struct XOTcl_Object * obj);
#endif
#ifndef XOTclDeleteClass_TCL_DECLARED
#define XOTclDeleteClass_TCL_DECLARED
/* 10 */
EXTERN int		XOTclDeleteClass (Tcl_Interp * interp, 
				struct XOTcl_Class * cl);
#endif
/* Slot 11 is reserved */
/* Slot 12 is reserved */
#ifndef XOTclRemovePMethod_TCL_DECLARED
#define XOTclRemovePMethod_TCL_DECLARED
/* 13 */
EXTERN int		XOTclRemovePMethod (Tcl_Interp * interp, 
				struct XOTcl_Object * obj, CONST char * nm);
#endif
#ifndef XOTclRemoveIMethod_TCL_DECLARED
#define XOTclRemoveIMethod_TCL_DECLARED
/* 14 */
EXTERN int		XOTclRemoveIMethod (Tcl_Interp * interp, 
				struct XOTcl_Class * cl, CONST char * nm);
#endif
#ifndef XOTclOSetInstVar_TCL_DECLARED
#define XOTclOSetInstVar_TCL_DECLARED
/* 15 */
EXTERN Tcl_Obj *	XOTclOSetInstVar (struct XOTcl_Object * obj, 
				Tcl_Interp * interp, Tcl_Obj * name, 
				Tcl_Obj * value, int flgs);
#endif
#ifndef XOTclOGetInstVar_TCL_DECLARED
#define XOTclOGetInstVar_TCL_DECLARED
/* 16 */
EXTERN Tcl_Obj *	XOTclOGetInstVar (struct XOTcl_Object * obj, 
				Tcl_Interp * interp, Tcl_Obj * name, 
				int flgs);
#endif
/* Slot 17 is reserved */
/* Slot 18 is reserved */
#ifndef XOTcl_ObjSetVar2_TCL_DECLARED
#define XOTcl_ObjSetVar2_TCL_DECLARED
/* 19 */
EXTERN Tcl_Obj *	XOTcl_ObjSetVar2 (struct XOTcl_Object * obj, 
				Tcl_Interp * interp, Tcl_Obj * name1, 
				Tcl_Obj * name2, Tcl_Obj * value, int flgs);
#endif
#ifndef XOTcl_ObjGetVar2_TCL_DECLARED
#define XOTcl_ObjGetVar2_TCL_DECLARED
/* 20 */
EXTERN Tcl_Obj *	XOTcl_ObjGetVar2 (struct XOTcl_Object * obj, 
				Tcl_Interp * interp, Tcl_Obj * name1, 
				Tcl_Obj * name2, int flgs);
#endif
#ifndef XOTclUnsetInstVar2_TCL_DECLARED
#define XOTclUnsetInstVar2_TCL_DECLARED
/* 21 */
EXTERN int		XOTclUnsetInstVar2 (struct XOTcl_Object * obj, 
				Tcl_Interp * interp, char * name1, 
				char * name2, int flgs);
#endif
/* Slot 22 is reserved */
#ifndef XOTclErrMsg_TCL_DECLARED
#define XOTclErrMsg_TCL_DECLARED
/* 23 */
EXTERN int		XOTclErrMsg (Tcl_Interp * interp, char * msg, 
				Tcl_FreeProc * type);
#endif
#ifndef XOTclVarErrMsg_TCL_DECLARED
#define XOTclVarErrMsg_TCL_DECLARED
/* 24 */
EXTERN int		XOTclVarErrMsg (Tcl_Interp * interp, ...);
#endif
#ifndef XOTclErrInProc_TCL_DECLARED
#define XOTclErrInProc_TCL_DECLARED
/* 25 */
EXTERN int		XOTclErrInProc (Tcl_Interp * interp, 
				Tcl_Obj * objName, Tcl_Obj * clName, 
				char * procName);
#endif
/* Slot 26 is reserved */
#ifndef XOTclErrBadVal__TCL_DECLARED
#define XOTclErrBadVal__TCL_DECLARED
/* 27 */
EXTERN int		XOTclErrBadVal_ (Tcl_Interp * interp, 
				char * expected, char * value);
#endif
#ifndef XOTclObjErrType_TCL_DECLARED
#define XOTclObjErrType_TCL_DECLARED
/* 28 */
EXTERN int		XOTclObjErrType (Tcl_Interp * interp, Tcl_Obj * nm, 
				char * wt);
#endif
#ifndef XOTclStackDump_TCL_DECLARED
#define XOTclStackDump_TCL_DECLARED
/* 29 */
EXTERN void		XOTclStackDump (Tcl_Interp * interp);
#endif
#ifndef XOTclCallStackDump_TCL_DECLARED
#define XOTclCallStackDump_TCL_DECLARED
/* 30 */
EXTERN void		XOTclCallStackDump (Tcl_Interp * interp);
#endif
/* Slot 31 is reserved */
#ifndef XOTclSetObjClientData_TCL_DECLARED
#define XOTclSetObjClientData_TCL_DECLARED
/* 32 */
EXTERN void		XOTclSetObjClientData (XOTcl_Object * obj, 
				ClientData data);
#endif
#ifndef XOTclGetObjClientData_TCL_DECLARED
#define XOTclGetObjClientData_TCL_DECLARED
/* 33 */
EXTERN ClientData	XOTclGetObjClientData (XOTcl_Object * obj);
#endif
#ifndef XOTclSetClassClientData_TCL_DECLARED
#define XOTclSetClassClientData_TCL_DECLARED
/* 34 */
EXTERN void		XOTclSetClassClientData (XOTcl_Class * cl, 
				ClientData data);
#endif
#ifndef XOTclGetClassClientData_TCL_DECLARED
#define XOTclGetClassClientData_TCL_DECLARED
/* 35 */
EXTERN ClientData	XOTclGetClassClientData (XOTcl_Class * cl);
#endif
#ifndef XOTclRequireObjNamespace_TCL_DECLARED
#define XOTclRequireObjNamespace_TCL_DECLARED
/* 36 */
EXTERN void		XOTclRequireObjNamespace (Tcl_Interp * interp, 
				XOTcl_Object * obj);
#endif
#ifndef XOTclErrBadVal_TCL_DECLARED
#define XOTclErrBadVal_TCL_DECLARED
/* 37 */
EXTERN int		XOTclErrBadVal (Tcl_Interp * interp, char * context, 
				char * expected, char * value);
#endif
#ifndef XOTclNextObjCmd_TCL_DECLARED
#define XOTclNextObjCmd_TCL_DECLARED
/* 38 */
EXTERN int		XOTclNextObjCmd (ClientData cd, Tcl_Interp * interp, 
				int objc, Tcl_Obj *CONST objv[]);
#endif
#ifndef XOTclCallMethodWithArgs_TCL_DECLARED
#define XOTclCallMethodWithArgs_TCL_DECLARED
/* 39 */
EXTERN int		XOTclCallMethodWithArgs (ClientData cd, 
				Tcl_Interp * interp, Tcl_Obj * method, 
				Tcl_Obj * arg, int objc, 
				Tcl_Obj *CONST objv[], int flags);
#endif
#ifndef XOTclObjErrArgCnt_TCL_DECLARED
#define XOTclObjErrArgCnt_TCL_DECLARED
/* 40 */
EXTERN int		XOTclObjErrArgCnt (Tcl_Interp * interp, 
				Tcl_Obj * cmdName, Tcl_Obj * methodName, 
				char * arglist);
#endif
#ifndef XOTclAddObjectMethod_TCL_DECLARED
#define XOTclAddObjectMethod_TCL_DECLARED
/* 41 */
EXTERN int		XOTclAddObjectMethod (Tcl_Interp * interp, 
				struct XOTcl_Object * obj, CONST char * nm, 
				Tcl_ObjCmdProc * proc, ClientData cd, 
				Tcl_CmdDeleteProc * dp, int flags);
#endif
#ifndef XOTclAddInstanceMethod_TCL_DECLARED
#define XOTclAddInstanceMethod_TCL_DECLARED
/* 42 */
EXTERN int		XOTclAddInstanceMethod (Tcl_Interp * interp, 
				struct XOTcl_Class * cl, CONST char * nm, 
				Tcl_ObjCmdProc * proc, ClientData cd, 
				Tcl_CmdDeleteProc * dp, int flags);
#endif
#ifndef XOTclCreate_TCL_DECLARED
#define XOTclCreate_TCL_DECLARED
/* 43 */
EXTERN int		XOTclCreate (Tcl_Interp * in, XOTcl_Class * class, 
				Tcl_Obj * name, ClientData data, int objc, 
				Tcl_Obj *CONST objv[]);
#endif

typedef struct XotclStubHooks {
    struct XotclIntStubs *xotclIntStubs;
} XotclStubHooks;

typedef struct XotclStubs {
    int magic;
    struct XotclStubHooks *hooks;

    int (*xotcl_Init) (Tcl_Interp * interp); /* 0 */
    void *reserved1;
    struct XOTcl_Class * (*xOTclIsClass) (Tcl_Interp * interp, ClientData cd); /* 2 */
    void *reserved3;
    struct XOTcl_Object * (*xOTclGetObject) (Tcl_Interp * interp, char * name); /* 4 */
    struct XOTcl_Class * (*xOTclGetClass) (Tcl_Interp * interp, char * name); /* 5 */
    int (*xOTclCreateObject) (Tcl_Interp * interp, Tcl_Obj * name, struct XOTcl_Class * cl); /* 6 */
    void *reserved7;
    int (*xOTclCreateClass) (Tcl_Interp * interp, Tcl_Obj * name, struct XOTcl_Class * cl); /* 8 */
    int (*xOTclDeleteObject) (Tcl_Interp * interp, struct XOTcl_Object * obj); /* 9 */
    int (*xOTclDeleteClass) (Tcl_Interp * interp, struct XOTcl_Class * cl); /* 10 */
    void *reserved11;
    void *reserved12;
    int (*xOTclRemovePMethod) (Tcl_Interp * interp, struct XOTcl_Object * obj, CONST char * nm); /* 13 */
    int (*xOTclRemoveIMethod) (Tcl_Interp * interp, struct XOTcl_Class * cl, CONST char * nm); /* 14 */
    Tcl_Obj * (*xOTclOSetInstVar) (struct XOTcl_Object * obj, Tcl_Interp * interp, Tcl_Obj * name, Tcl_Obj * value, int flgs); /* 15 */
    Tcl_Obj * (*xOTclOGetInstVar) (struct XOTcl_Object * obj, Tcl_Interp * interp, Tcl_Obj * name, int flgs); /* 16 */
    void *reserved17;
    void *reserved18;
    Tcl_Obj * (*xOTcl_ObjSetVar2) (struct XOTcl_Object * obj, Tcl_Interp * interp, Tcl_Obj * name1, Tcl_Obj * name2, Tcl_Obj * value, int flgs); /* 19 */
    Tcl_Obj * (*xOTcl_ObjGetVar2) (struct XOTcl_Object * obj, Tcl_Interp * interp, Tcl_Obj * name1, Tcl_Obj * name2, int flgs); /* 20 */
    int (*xOTclUnsetInstVar2) (struct XOTcl_Object * obj, Tcl_Interp * interp, char * name1, char * name2, int flgs); /* 21 */
    void *reserved22;
    int (*xOTclErrMsg) (Tcl_Interp * interp, char * msg, Tcl_FreeProc * type); /* 23 */
    int (*xOTclVarErrMsg) (Tcl_Interp * interp, ...); /* 24 */
    int (*xOTclErrInProc) (Tcl_Interp * interp, Tcl_Obj * objName, Tcl_Obj * clName, char * procName); /* 25 */
    void *reserved26;
    int (*xOTclErrBadVal_) (Tcl_Interp * interp, char * expected, char * value); /* 27 */
    int (*xOTclObjErrType) (Tcl_Interp * interp, Tcl_Obj * nm, char * wt); /* 28 */
    void (*xOTclStackDump) (Tcl_Interp * interp); /* 29 */
    void (*xOTclCallStackDump) (Tcl_Interp * interp); /* 30 */
    void *reserved31;
    void (*xOTclSetObjClientData) (XOTcl_Object * obj, ClientData data); /* 32 */
    ClientData (*xOTclGetObjClientData) (XOTcl_Object * obj); /* 33 */
    void (*xOTclSetClassClientData) (XOTcl_Class * cl, ClientData data); /* 34 */
    ClientData (*xOTclGetClassClientData) (XOTcl_Class * cl); /* 35 */
    void (*xOTclRequireObjNamespace) (Tcl_Interp * interp, XOTcl_Object * obj); /* 36 */
    int (*xOTclErrBadVal) (Tcl_Interp * interp, char * context, char * expected, char * value); /* 37 */
    int (*xOTclNextObjCmd) (ClientData cd, Tcl_Interp * interp, int objc, Tcl_Obj *CONST objv[]); /* 38 */
    int (*xOTclCallMethodWithArgs) (ClientData cd, Tcl_Interp * interp, Tcl_Obj * method, Tcl_Obj * arg, int objc, Tcl_Obj *CONST objv[], int flags); /* 39 */
    int (*xOTclObjErrArgCnt) (Tcl_Interp * interp, Tcl_Obj * cmdName, Tcl_Obj * methodName, char * arglist); /* 40 */
    int (*xOTclAddObjectMethod) (Tcl_Interp * interp, struct XOTcl_Object * obj, CONST char * nm, Tcl_ObjCmdProc * proc, ClientData cd, Tcl_CmdDeleteProc * dp, int flags); /* 41 */
    int (*xOTclAddInstanceMethod) (Tcl_Interp * interp, struct XOTcl_Class * cl, CONST char * nm, Tcl_ObjCmdProc * proc, ClientData cd, Tcl_CmdDeleteProc * dp, int flags); /* 42 */
    int (*xOTclCreate) (Tcl_Interp * in, XOTcl_Class * class, Tcl_Obj * name, ClientData data, int objc, Tcl_Obj *CONST objv[]); /* 43 */
} XotclStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern XotclStubs *xotclStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_XOTCL_STUBS) && !defined(USE_XOTCL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#ifndef Xotcl_Init
#define Xotcl_Init \
	(xotclStubsPtr->xotcl_Init) /* 0 */
#endif
/* Slot 1 is reserved */
#ifndef XOTclIsClass
#define XOTclIsClass \
	(xotclStubsPtr->xOTclIsClass) /* 2 */
#endif
/* Slot 3 is reserved */
#ifndef XOTclGetObject
#define XOTclGetObject \
	(xotclStubsPtr->xOTclGetObject) /* 4 */
#endif
#ifndef XOTclGetClass
#define XOTclGetClass \
	(xotclStubsPtr->xOTclGetClass) /* 5 */
#endif
#ifndef XOTclCreateObject
#define XOTclCreateObject \
	(xotclStubsPtr->xOTclCreateObject) /* 6 */
#endif
/* Slot 7 is reserved */
#ifndef XOTclCreateClass
#define XOTclCreateClass \
	(xotclStubsPtr->xOTclCreateClass) /* 8 */
#endif
#ifndef XOTclDeleteObject
#define XOTclDeleteObject \
	(xotclStubsPtr->xOTclDeleteObject) /* 9 */
#endif
#ifndef XOTclDeleteClass
#define XOTclDeleteClass \
	(xotclStubsPtr->xOTclDeleteClass) /* 10 */
#endif
/* Slot 11 is reserved */
/* Slot 12 is reserved */
#ifndef XOTclRemovePMethod
#define XOTclRemovePMethod \
	(xotclStubsPtr->xOTclRemovePMethod) /* 13 */
#endif
#ifndef XOTclRemoveIMethod
#define XOTclRemoveIMethod \
	(xotclStubsPtr->xOTclRemoveIMethod) /* 14 */
#endif
#ifndef XOTclOSetInstVar
#define XOTclOSetInstVar \
	(xotclStubsPtr->xOTclOSetInstVar) /* 15 */
#endif
#ifndef XOTclOGetInstVar
#define XOTclOGetInstVar \
	(xotclStubsPtr->xOTclOGetInstVar) /* 16 */
#endif
/* Slot 17 is reserved */
/* Slot 18 is reserved */
#ifndef XOTcl_ObjSetVar2
#define XOTcl_ObjSetVar2 \
	(xotclStubsPtr->xOTcl_ObjSetVar2) /* 19 */
#endif
#ifndef XOTcl_ObjGetVar2
#define XOTcl_ObjGetVar2 \
	(xotclStubsPtr->xOTcl_ObjGetVar2) /* 20 */
#endif
#ifndef XOTclUnsetInstVar2
#define XOTclUnsetInstVar2 \
	(xotclStubsPtr->xOTclUnsetInstVar2) /* 21 */
#endif
/* Slot 22 is reserved */
#ifndef XOTclErrMsg
#define XOTclErrMsg \
	(xotclStubsPtr->xOTclErrMsg) /* 23 */
#endif
#ifndef XOTclVarErrMsg
#define XOTclVarErrMsg \
	(xotclStubsPtr->xOTclVarErrMsg) /* 24 */
#endif
#ifndef XOTclErrInProc
#define XOTclErrInProc \
	(xotclStubsPtr->xOTclErrInProc) /* 25 */
#endif
/* Slot 26 is reserved */
#ifndef XOTclErrBadVal_
#define XOTclErrBadVal_ \
	(xotclStubsPtr->xOTclErrBadVal_) /* 27 */
#endif
#ifndef XOTclObjErrType
#define XOTclObjErrType \
	(xotclStubsPtr->xOTclObjErrType) /* 28 */
#endif
#ifndef XOTclStackDump
#define XOTclStackDump \
	(xotclStubsPtr->xOTclStackDump) /* 29 */
#endif
#ifndef XOTclCallStackDump
#define XOTclCallStackDump \
	(xotclStubsPtr->xOTclCallStackDump) /* 30 */
#endif
/* Slot 31 is reserved */
#ifndef XOTclSetObjClientData
#define XOTclSetObjClientData \
	(xotclStubsPtr->xOTclSetObjClientData) /* 32 */
#endif
#ifndef XOTclGetObjClientData
#define XOTclGetObjClientData \
	(xotclStubsPtr->xOTclGetObjClientData) /* 33 */
#endif
#ifndef XOTclSetClassClientData
#define XOTclSetClassClientData \
	(xotclStubsPtr->xOTclSetClassClientData) /* 34 */
#endif
#ifndef XOTclGetClassClientData
#define XOTclGetClassClientData \
	(xotclStubsPtr->xOTclGetClassClientData) /* 35 */
#endif
#ifndef XOTclRequireObjNamespace
#define XOTclRequireObjNamespace \
	(xotclStubsPtr->xOTclRequireObjNamespace) /* 36 */
#endif
#ifndef XOTclErrBadVal
#define XOTclErrBadVal \
	(xotclStubsPtr->xOTclErrBadVal) /* 37 */
#endif
#ifndef XOTclNextObjCmd
#define XOTclNextObjCmd \
	(xotclStubsPtr->xOTclNextObjCmd) /* 38 */
#endif
#ifndef XOTclCallMethodWithArgs
#define XOTclCallMethodWithArgs \
	(xotclStubsPtr->xOTclCallMethodWithArgs) /* 39 */
#endif
#ifndef XOTclObjErrArgCnt
#define XOTclObjErrArgCnt \
	(xotclStubsPtr->xOTclObjErrArgCnt) /* 40 */
#endif
#ifndef XOTclAddObjectMethod
#define XOTclAddObjectMethod \
	(xotclStubsPtr->xOTclAddObjectMethod) /* 41 */
#endif
#ifndef XOTclAddInstanceMethod
#define XOTclAddInstanceMethod \
	(xotclStubsPtr->xOTclAddInstanceMethod) /* 42 */
#endif
#ifndef XOTclCreate
#define XOTclCreate \
	(xotclStubsPtr->xOTclCreate) /* 43 */
#endif

#endif /* defined(USE_XOTCL_STUBS) && !defined(USE_XOTCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */

#endif /* _XOTCLDECLS */

