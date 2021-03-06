/*
 * nsfError.c --
 *
 *      Error reporting functions for the Next Scripting Framework.
 *
 * Copyright (C) 1999-2015 Gustaf Neumann (a, b)
 * Copyright (C) 1999-2007 Uwe Zdun (a, b)
 * Copyright (C) 2011-2014 Stefan Sobernig (b)
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
 * This work is licensed under the MIT License http://www.opensource.org/licenses/MIT
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
 */

#include "nsfInt.h"

/* function prototypes */
Tcl_Obj *NsfParamDefsSyntax(Tcl_Interp *interp, Nsf_Param const *paramsPtr,
			    NsfObject *contextObject, const char *pattern)
  nonnull(1) nonnull(2) returns_nonnull;


/*
 *----------------------------------------------------------------------
 *
 * NsfDStringVPrintf --
 *
 *      Appends to a Tcl_DString a formatted value. This function
 *      iterates until it has sufficiently memory allocated.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
NsfDStringVPrintf(Tcl_DString *dsPtr, const char *fmt, va_list vargs) {
  int      result, failure, offset = dsPtr->length, avail = dsPtr->spaceAvl;
  va_list  vargsCopy;

  /*
   * Work on a copy of the va_list so that the caller's copy is untouched
   */
  va_copy(vargsCopy, vargs);
  result = vsnprintf(dsPtr->string + offset, avail, fmt, vargsCopy);
  va_end(vargsCopy);

  /*
   * Trap C99+ incompatabilities of certain vsnprintf() implementations
   * w.r.t. the result value: For example, old *nix implementations of
   * vsnprintf() as well as C89 implementations (as current MS Visual Compiler
   * runtimes) return -1 (or another negative number) upon overflowing the
   * buffer (rather than the number of required bytes as required by C99) and
   * upon other error conditions. This should not happen for the above size
   * estimation, however. Also, for MS VC runtimes, we use the vendor-specific
   * _vscprintf()
   *
   * Note: For MinGW and MinGW-w64, we assume that their ANSI-compliant
   * version of vsnprintf() is used. See __USE_MINGW_ANSI_STDIO in nsfInt.h
   */

#if defined(_MSC_VER)
  failure = (result == -1 && errno == ERANGE) || (result == avail) /* VC 12 */;
#else
  assert(result > -1);
  failure = (result >= avail);
#endif

  if (likely(failure == 0)) {
    /*
     * vsnprintf() has already copied all content,
     * we have just to adjust the length.
     */
    Tcl_DStringSetLength(dsPtr, offset + result);
  } else {
    int addedStringLength;
    /*
     * vsnprintf() has already not copied all content,
     * we have to determine the required length (MS),
     * adjust the DString size and copy again.
     */
#if defined(_MSC_VER)
    va_copy(vargsCopy, vargs);
    addedStringLength = _vscprintf(fmt, vargsCopy);
    va_end(vargsCopy);
#else
    addedStringLength = result;
#endif
    Tcl_DStringSetLength(dsPtr, offset + addedStringLength);

    va_copy(vargsCopy, vargs);
    result = vsnprintf(dsPtr->string + offset, dsPtr->spaceAvl, fmt, vargsCopy);
    assert(result > -1);
    va_end(vargsCopy);
  }
}

/*
 *----------------------------------------------------------------------
 * Nsf_DStringPrintf --
 *
 *      Append a sequence of values using a format string.
 *
 * Results:
 *      Pointer to the current string value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Nsf_DStringPrintf(Tcl_DString *dsPtr, const char *fmt, ...)
{
    va_list         ap;

    nonnull_assert(dsPtr != NULL);
    nonnull_assert(fmt != NULL);

    va_start(ap, fmt);
    NsfDStringVPrintf(dsPtr, fmt, ap);
    va_end(ap);
}

/*
 *----------------------------------------------------------------------
 *
 * NsfDStringArgv --
 *
 *      Appends argument vector to an initialized Tcl_DString.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
NsfDStringArgv(Tcl_DString *dsPtr, int objc, Tcl_Obj *CONST objv[]) {

  nonnull_assert(dsPtr != NULL);
  nonnull_assert(objv != NULL);

  if (objc > 0) {
    int i;
    Tcl_DStringAppendElement(dsPtr, NsfMethodName(objv[0]));
    for (i = 1; i < objc; i++) {
      Tcl_DStringAppendElement(dsPtr, ObjStr(objv[i]));
    }
  }
}


/*
 *----------------------------------------------------------------------
 *
 * NsfPrintError --
 *
 *      Produce a formatted error message with a printf like semantics
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfPrintError(Tcl_Interp *interp, const char *fmt, ...) {
  va_list ap;
  Tcl_DString ds;

  Tcl_DStringInit(&ds);

  va_start(ap, fmt);
  NsfDStringVPrintf(&ds, fmt, ap);
  va_end(ap);

  Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_DStringValue(&ds), -1));
  Tcl_DStringFree(&ds);

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfErrInProc --
 *
 *      Produce a general error message when an error occurs in a scripted nsf
 *      method.
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfErrInProc(Tcl_Interp *interp, Tcl_Obj *objName,
               Tcl_Obj *clName, const char *procName) {
  Tcl_DString errMsg;
  char *cName, *space;

  Tcl_DStringInit(&errMsg);
  Tcl_DStringAppend(&errMsg, "\n    ", -1);
  if (clName != NULL) {
    cName = ObjStr(clName);
    space = " ";
  } else {
    cName = "";
    space ="";
  }
  Tcl_DStringAppend(&errMsg, ObjStr(objName),-1);
  Tcl_DStringAppend(&errMsg, space, -1);
  Tcl_DStringAppend(&errMsg, cName, -1);
  Tcl_DStringAppend(&errMsg, "->", 2);
  Tcl_DStringAppend(&errMsg, procName, -1);
  Tcl_AddErrorInfo (interp, Tcl_DStringValue(&errMsg));
  Tcl_DStringFree(&errMsg);
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfObjWrongArgs --
 *
 *      Produce a general error message when a nsf method is called with an
 *      invalid argument list (wrong number of arguments).
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfObjWrongArgs(Tcl_Interp *interp, const char *msg, Tcl_Obj *cmdNameObj,
		Tcl_Obj *methodPathObj, char *arglist) {
  int need_space = 0;

  nonnull_assert(interp != NULL);
  nonnull_assert(msg != NULL);

  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, msg, " should be \"", (char *) NULL);
  if (cmdNameObj != NULL) {
    Tcl_AppendResult(interp, ObjStr(cmdNameObj), (char *) NULL);
    need_space = 1;
  }

  if (methodPathObj != NULL) {
    if (need_space != 0) {
      Tcl_AppendResult(interp, " ", (char *) NULL);
    }

    INCR_REF_COUNT(methodPathObj);
    Tcl_AppendResult(interp, ObjStr(methodPathObj), (char *) NULL);
    DECR_REF_COUNT(methodPathObj);

    need_space = 1;
  }
  if (arglist != NULL) {
    if (need_space != 0) {
      Tcl_AppendResult(interp, " ", (char *) NULL);
    }
    Tcl_AppendResult(interp, arglist, (char *) NULL);
  }
  Tcl_AppendResult(interp, "\"", (char *) NULL);
  return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * NsfArgumentError --
 *
 *      Produce a wrong number of argument error based on a parameter definition
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfArgumentError(Tcl_Interp *interp, const char *errorMsg, Nsf_Param const *paramPtr,
              Tcl_Obj *cmdNameObj, Tcl_Obj *methodPathObj) {
  Tcl_Obj *argStringObj = NsfParamDefsSyntax(interp, paramPtr, NULL, NULL);

  nonnull_assert(interp != NULL);
  nonnull_assert(errorMsg != NULL);
  nonnull_assert(paramPtr != NULL);

  NsfObjWrongArgs(interp, errorMsg, cmdNameObj, methodPathObj, ObjStr(argStringObj));
  DECR_REF_COUNT2("paramDefsObj", argStringObj);

  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfUnexpectedArgumentError --
 *
 *      Produce an error message about an unexpected argument (most likely,
 *      too many arguments)
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfUnexpectedArgumentError(Tcl_Interp *interp, const char *argumentString,
			   Nsf_Object *object, Nsf_Param const *paramPtr,
			   Tcl_Obj *methodPathObj) {
  Tcl_DString ds, *dsPtr = &ds;

  nonnull_assert(interp != NULL);
  nonnull_assert(argumentString != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(methodPathObj != NULL);

  DSTRING_INIT(dsPtr);
  Tcl_DStringAppend(dsPtr, "invalid argument '", -1);
  Tcl_DStringAppend(dsPtr, argumentString, -1);
  Tcl_DStringAppend(dsPtr, "', maybe too many arguments;", -1);
  NsfArgumentError(interp, Tcl_DStringValue(dsPtr), paramPtr, (object != NULL) ? object->cmdName : NULL,
		   methodPathObj);
  DSTRING_FREE(dsPtr);
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfUnexpectedNonposArgumentError --
 *
 *      Produce an error message about an invalid nonposistional argument.
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfUnexpectedNonposArgumentError(Tcl_Interp *interp,
				 const char *argumentString,
				 Nsf_Object *object,
				 Nsf_Param const *currentParamPtr,
				 Nsf_Param const *paramPtr,
				 Tcl_Obj *methodPathObj) {
  Tcl_DString ds, *dsPtr = &ds;
  Nsf_Param const *pPtr;

  nonnull_assert(interp != NULL);
  nonnull_assert(argumentString != NULL);
  nonnull_assert(currentParamPtr != NULL);
  nonnull_assert(paramPtr != NULL);
  nonnull_assert(methodPathObj != NULL);

  DSTRING_INIT(dsPtr);
  Tcl_DStringAppend(dsPtr, "invalid non-positional argument '", -1);
  Tcl_DStringAppend(dsPtr, argumentString, -1);
  Tcl_DStringAppend(dsPtr, "', valid are : ", -1);
  for (pPtr = currentParamPtr; pPtr->name && *pPtr->name == '-'; pPtr ++) {
    if (pPtr->flags & NSF_ARG_NOCONFIG) {
      continue;
    }
    Tcl_DStringAppend(dsPtr, pPtr->name, -1);
    Tcl_DStringAppend(dsPtr, ", ", -1);
  }
  Tcl_DStringTrunc(dsPtr, Tcl_DStringLength(dsPtr) - 2);
  Tcl_DStringAppend(dsPtr, ";\n", 2);

  NsfArgumentError(interp, Tcl_DStringValue(dsPtr), paramPtr, (object != NULL) ? object->cmdName : NULL,
		   methodPathObj);
  DSTRING_FREE(dsPtr);
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NsfDispatchClientDataError --
 *
 *      Produce a error message when method was not dispatched on an object
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfDispatchClientDataError(Tcl_Interp *interp, ClientData clientData,
			   const char *what, const char *methodName) {

  nonnull_assert(interp != NULL);
  nonnull_assert(what != NULL);
  nonnull_assert(methodName != NULL);

  if (clientData != NULL) {
    return NsfPrintError(interp, "method %s not dispatched on valid %s",
			 methodName, what);
  } else {
    return NsfNoCurrentObjectError(interp, methodName);
  }
}

/*
 *----------------------------------------------------------------------
 *
 * NsfNoCurrentObjectError --
 *
 *      Produce a error message when method was called outside the context of
 *      a method
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfNoCurrentObjectError(Tcl_Interp *interp, const char *what) {

  nonnull_assert(interp != NULL);

  return NsfPrintError(interp, "no current object; %s called outside the context of a Next Scripting method",
                       (what != NULL) ? what : "command");
}

/*
 *----------------------------------------------------------------------
 *
 * NsfObjErrType --
 *
 *      Produce a general error message when a nsf method is called with an
 *      invalid value for some argument.
 *
 * Results:
 *      TCL_ERROR
 *
 * Side effects:
 *      Sets the result message.
 *
 *----------------------------------------------------------------------
 */
int
NsfObjErrType(Tcl_Interp *interp,
	      const char *context,
	      Tcl_Obj *value,
	      const char *type,
	      Nsf_Param const *paramPtr)
{
  int named = (paramPtr && (paramPtr->flags & NSF_ARG_UNNAMED) == 0);
  int returnValue = !named && paramPtr && (paramPtr->flags & NSF_ARG_IS_RETURNVALUE);
  char *prevErrMsg = ObjStr(Tcl_GetObjResult(interp));

  if (*prevErrMsg != '\0') {
    Tcl_AppendResult(interp, " 2nd error: ",  (char *) NULL);
  }

  /*Tcl_ResetResult(interp);*/
  if (context != NULL) {
    Tcl_AppendResult(interp, context, ": ",  (char *) NULL);
  }
  Tcl_AppendResult(interp,"expected ", type, " but got \"",  ObjStr(value), "\"", (char *) NULL);
  if (named != 0) {
    Tcl_AppendResult(interp," for parameter \"", paramPtr->name, "\"", (char *) NULL);
  } else if (returnValue != 0) {
    Tcl_AppendResult(interp," as return value", (char *) NULL);
  }
  return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 78
 * indent-tabs-mode: nil
 * End:
 */
