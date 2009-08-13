#if !defined(TCL85STACK)

XOTCLINLINE static XOTclObject*
GetSelfObj(Tcl_Interp *interp) {
  return CallStackGetFrame(interp)->self;
}

static XOTclCallStackContent*
CallStackGetFrame(Tcl_Interp *interp) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  register XOTclCallStackContent *top = cs->top;
  Tcl_CallFrame *varFramePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp);
  
  /*fprintf(stderr, "framePtr %p != varFramePtr %p && top->currentFramePtr %p => %d\n", 
          Tcl_Interp_framePtr(interp), varFramePtr, top->currentFramePtr,
          (Tcl_Interp_framePtr(interp) != varFramePtr && top->currentFramePtr)
          );*/

  if (Tcl_Interp_framePtr(interp) != varFramePtr && top->currentFramePtr) {
    XOTclCallStackContent *bot = cs->content + 1;

    /* we are in a uplevel */
    while (varFramePtr != top->currentFramePtr && top>bot) {
      top--;
    }
  }

  return top;
}

XOTCLINLINE static XOTclCallStackContent*
CallStackGetTopFrame(Tcl_Interp *interp) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  return cs->top;
}

XOTclCallStackContent *
XOTclCallStackFindLastInvocation(Tcl_Interp *interp, int offset, Tcl_CallFrame **framePtrPtr) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  register XOTclCallStackContent *csc = cs->top;
  int topLevel = csc->currentFramePtr ? Tcl_CallFrame_level(csc->currentFramePtr) : 0;
  int deeper = offset;

  /* skip through toplevel inactive filters, do this offset times */
  for (csc=cs->top; csc > cs->content; csc--) {
    /* fprintf(stderr, "csc %p callType = %x, frameType = %x, offset=%d\n",
       csc,csc->callType,csc->frameType,offset); */
    if ((csc->callType & XOTCL_CSC_CALL_IS_NEXT) ||
        (csc->frameType & XOTCL_CSC_TYPE_INACTIVE))
      continue;
    if (offset)
      offset--;
    else {
      /* fprintf(stderr, "csc %p offset ok, deeper=%d\n",csc,deeper); */
      if (!deeper || cs->top->callType & XOTCL_CSC_CALL_IS_GUARD) {
        if (framePtrPtr) *framePtrPtr = csc->currentFramePtr;
        return csc;
      }
      if (csc->currentFramePtr && Tcl_CallFrame_level(csc->currentFramePtr) < topLevel) {
        if (framePtrPtr) *framePtrPtr = csc->currentFramePtr;
        return csc;
      }
    }
  }
  /* for some reasons, we could not find invocation (topLevel, destroy) */
  /* fprintf(stderr, "csc %p could not find invocation\n",csc);*/
  if (framePtrPtr) *framePtrPtr = NULL;
  return NULL;
}

XOTclCallStackContent *
XOTclCallStackFindActiveFrame(Tcl_Interp *interp, int offset, Tcl_CallFrame **framePtrPtr) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  register XOTclCallStackContent *csc;

  /* search for first active frame and set tcl frame pointers */
  for (csc=cs->top-offset; csc > cs->content; csc --) {
    if (!(csc->frameType & XOTCL_CSC_TYPE_INACTIVE)) {
      /* we found the highest active frame */
      if (framePtrPtr) *framePtrPtr = csc->currentFramePtr;
      return csc;
    }
  }
  /* we could not find an active frame; called from toplevel? */
  if (framePtrPtr) *framePtrPtr = NULL;
  return NULL;
}

static void
CallStackUseActiveFrames(Tcl_Interp *interp, callFrameContext *ctx) {
  XOTclCallStackContent *active, *top;
  Tcl_CallFrame *inFramePtr = (Tcl_CallFrame *)Tcl_Interp_varFramePtr(interp), 
    *varFramePtr, *activeFramePtr, *framePtr;

  active = XOTclCallStackFindActiveFrame(interp, 0, &activeFramePtr);
  top = CallStackGetTopFrame(interp);
  varFramePtr = inFramePtr;

  /*fprintf(stderr,"CallStackUseActiveFrames inframe %p varFrame %p activeFrame %p lvl %d\n",
    inFramePtr,varFramePtr,activeFramePtr, Tcl_CallFrame_level(inFramePtr));*/

  if (activeFramePtr == varFramePtr || active == top || Tcl_CallFrame_level(inFramePtr) == 0) {
    /* top frame is a active frame, or we could not find a calling frame */
    framePtr = varFramePtr;

  } else if (active == NULL) {
    /* There is no xotcl callframe active; use the caller of inframe */
    fprintf(stderr,"active == NULL\n");
    for (framePtr = inFramePtr; framePtr && Tcl_CallFrame_level(framePtr); framePtr = Tcl_CallFrame_callerPtr(framePtr)) {
      if (framePtr != top->currentFramePtr)
        break;
    }
  } else {
    /* The active framePtr is an entry deeper in the stack. When XOTcl
       is interleaved with Tcl, we return the Tcl frame */

    /* fprintf(stderr,"active == deeper, use Tcl frame\n"); */
    if ((framePtr = (active+1)->currentFramePtr)) {
      framePtr = Tcl_CallFrame_callerPtr(framePtr);
    } else {
      framePtr = active->currentFramePtr;
    }
  }
  if (inFramePtr == framePtr) {
    /* call frame pointers are fine */
    /*fprintf(stderr, "... no need to save frames\n");*/
    ctx->framesSaved = 0;
  } else {
    ctx->varFramePtr = inFramePtr;
    Tcl_Interp_varFramePtr(interp) = (CallFrame *)framePtr;
    ctx->framesSaved = 1;
  }
}

static XOTclCallStackContent *
CallStackFindActiveFilter(Tcl_Interp *interp) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  register XOTclCallStackContent *csc;

  /* search for first active frame and set tcl frame pointers */
  for (csc=cs->top; csc > cs->content; csc --) {
    if (csc->frameType == XOTCL_CSC_TYPE_ACTIVE_FILTER) return csc;
  }
  /* for some reasons, we could not find invocation (topLevel, destroy) */
  return NULL;
}

static void 
CallStackClearCmdReferences(Tcl_Interp *interp, Tcl_Command cmd) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  XOTclCallStackContent *csc = cs->top;

  for (; csc > cs->content; csc--) {
    if (csc->cmdPtr == cmd) {
      csc->cmdPtr = NULL;
    }
  }
}

static XOTclCallStackContent* 
CallStackGetObjectFrame(Tcl_Interp *interp, XOTclObject *obj) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  XOTclCallStackContent *csc = CallStackGetTopFrame(interp);

  for (; csc >= cs->content; csc--) {
    if (csc->self == obj) {
      return csc;
    }
  }
  return NULL;
}

static int
CallStackMarkDestroyed(Tcl_Interp *interp, XOTclObject *obj) {
  XOTclCallStack *cs = &RUNTIME_STATE(interp)->cs;
  XOTclCallStackContent *csc;
  int countSelfs = 0;
  Tcl_Command oid = obj->id;

  for (csc = &cs->content[1]; csc <= cs->top; csc++) {
    if (csc->self == obj) {
      csc->destroyedCmd = oid;
      csc->callType |= XOTCL_CSC_CALL_IS_DESTROY;
      /*fprintf(stderr,"setting destroy on csc %p for obj %p\n", csc, obj);*/
      if (csc->destroyedCmd) {
        Tcl_Command_refCount(csc->destroyedCmd)++;
        MEM_COUNT_ALLOC("command refCount", csc->destroyedCmd);
      }
      countSelfs++;
    }
  }
  return countSelfs;
}
#endif /* NOT TCL85STACK */


