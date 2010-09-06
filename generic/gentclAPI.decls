# -*- Tcl -*-
#
# This script is sourced by the C-code generator gentclAPI.tcl in the
# same directory. It is also used by the nx::doc toolkit to generate
# the authorative language reference documents.

# namespaces for types of methods
array set ns {
  nsfCmd    "::nsf"
  objectMethod "::nsf::cmd::Object"
  objectInfoMethod "::nsf::cmd::ObjectInfo"
  classMethod  "::nsf::cmd::Class"
  classInfoMethod "::nsf::cmd::ClassInfo"
  checkMethod  "::nsf::cmd::ParameterType"
}

#
# XOTcl commands
#
nsfCmd alias NsfAliasCmd {
  {-argName "object" -type object}
  {-argName "-per-object"}
  {-argName "methodName"}
  {-argName "-nonleaf"}
  {-argName "-objscope"}
  {-argName "cmdName" -required 1 -type tclobj}
}
nsfCmd assertion NsfAssertionCmd {
  {-argName "object" -type object}
  {-argName "assertionsubcmd" -required 1 -type "check|object-invar|class-invar"}
  {-argName "arg" -required 0 -type tclobj}
}

nsfCmd configure NsfConfigureCmd {
  {-argName "configureoption" -required 1 -type "filter|softrecreate|objectsystems|keepinitcmd|checkresults|checkarguments"}
  {-argName "value" -required 0 -type tclobj}
}
nsfCmd createobjectsystem NsfCreateObjectSystemCmd {
  {-argName "rootClass" -required 1 -type tclobj}
  {-argName "rootMetaClass" -required 1 -type tclobj}
  {-argName "systemMethods" -required 0 -type tclobj}
}
nsfCmd deprecated NsfDeprecatedCmd {
  {-argName "what" -required 1}
  {-argName "oldCmd" -required 1}
  {-argName "newCmd" -required 0}
}
nsfCmd dispatch NsfDispatchCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-objscope"}
  {-argName "command" -required 1 -type tclobj}
  {-argName "args"  -type args}
}
nsfCmd colon NsfColonCmd {
  {-argName "args" -type allargs}
}
nsfCmd existsvar NsfExistsVarCmd {
  {-argName "object" -required 1 -type object}
  {-argName "var" -required 1}
}
nsfCmd finalize NsfFinalizeObjCmd {
}

nsfCmd forward NsfForwardCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "method" -required 1 -type tclobj}
  {-argName "-default" -nrargs 1 -type tclobj}
  {-argName "-earlybinding"}
  {-argName "-methodprefix" -nrargs 1 -type tclobj}
  {-argName "-objscope"}
  {-argName "-onerror" -nrargs 1 -type tclobj}
  {-argName "-verbose"}
  {-argName "target" -type tclobj}
  {-argName "args" -type args}
}
nsfCmd importvar NsfImportvarCmd {
  {-argName "object" -type object}
  {-argName "args" -type args}
}
nsfCmd interp NsfInterpObjCmd {
  {-argName "name"}
  {-argName "args" -type allargs}
}
nsfCmd invalidateobjectparameter NsfInvalidateObjectParameterCmd {
  {-argName "class" -type class}
}
nsfCmd is NsfIsCmd {
  {-argName "-complain"}
  {-argName "constraint" -required 1 -type tclobj}
  {-argName "value" -required 1 -type tclobj}
}
nsfCmd isobject NsfIsObjectCmd {
  {-argName "object" -required 1 -type tclobj}
}
nsfCmd method NsfMethodCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-inner-namespace"}
  {-argName "-per-object"}
  {-argName "-public"}
  {-argName "name" -required 1 -type tclobj}
  {-argName "args" -required 1 -type tclobj}
  {-argName "body" -required 1 -type tclobj}
  {-argName "-precondition"  -nrargs 1 -type tclobj}
  {-argName "-postcondition" -nrargs 1 -type tclobj}
}
nsfCmd methodproperty NsfMethodPropertyCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "methodName" -required 1 -type tclobj}
  {-argName "methodproperty" -required 1 -type "class-only|protected|redefine-protected|returns|slotobj"}
  {-argName "value" -type tclobj}
}
nsfCmd my NsfMyCmd {
  {-argName "-local"}
  {-argName "method" -required 1 -type tclobj}
  {-argName "args" -type args}
}

nsfCmd namespace_copycmds NsfNSCopyCmds {
  {-argName "fromNs" -required 1 -type tclobj}
  {-argName "toNs" -required 1 -type tclobj}
}
nsfCmd namespace_copyvars NsfNSCopyVars {
  {-argName "fromNs" -required 1 -type tclobj}
  {-argName "toNs" -required 1 -type tclobj}
}
nsfCmd __qualify NsfQualifyObjCmd {
  {-argName "name" -required 1 -type tclobj}
}
nsfCmd relation NsfRelationCmd {
  {-argName "object" -type object}
  {-argName "relationtype" -required 1 -type "object-mixin|class-mixin|object-filter|class-filter|class|superclass|rootclass"}
  {-argName "value" -required 0 -type tclobj}
}
nsfCmd current NsfCurrentCmd {
  {-argName "currentoption" -required 0 -type "proc|method|object|class|activelevel|args|activemixin|calledproc|calledmethod|calledclass|callingproc|callingmethod|callingclass|callinglevel|callingobject|filterreg|isnextcall|next"}
}

nsfCmd setvar NsfSetVarCmd {
  {-argName "object" -required 1 -type object}
  {-argName "variable" -required 1 -type tclobj}
  {-argName "value" -required 0 -type tclobj}
}
nsfCmd setter NsfSetterCmd {
  {-argName "object" -required 1 -type object}
  {-argName "-per-object"}
  {-argName "parameter" -type tclobj}
}

objectMethod autoname NsfOAutonameMethod {
  {-argName "-instance"}
  {-argName "-reset"}
  {-argName "name" -required 1 -type tclobj}
}

objectMethod cleanup NsfOCleanupMethod {
}

objectMethod configure NsfOConfigureMethod {
  {-argName "args" -type allargs}
}

objectMethod destroy NsfODestroyMethod {
}

objectMethod exists NsfOExistsMethod {
  {-argName "var" -required 1}
}

objectMethod filterguard NsfOFilterGuardMethod {
  {-argName "filter" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}

objectMethod instvar NsfOInstVarMethod {
  {-argName "args" -type allargs}
}

objectMethod mixinguard NsfOMixinGuardMethod {
  {-argName "mixin" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}

# objectMethod __next NsfONextMethod {
#  {-argName "args" -type allargs}
#}

objectMethod noinit NsfONoinitMethod {
}

objectMethod requireNamespace NsfORequireNamespaceMethod {
}

objectMethod residualargs NsfOResidualargsMethod {
  {-argName "args" -type allargs}
}

objectMethod uplevel NsfOUplevelMethod {
  {-argName "args" -type allargs}
}

objectMethod upvar NsfOUpvarMethod {
  {-argName "args" -type allargs}
}

objectMethod volatile NsfOVolatileMethod {
}

objectMethod vwait NsfOVwaitMethod {
  {-argName "varname" -required 1}
}

#
# info object methods
#
objectInfoMethod callable NsfObjInfoCallableMethod {
  {-argName "infocallablesubcmd" -nrargs 1 -type "filter|method|methods" -required 1}
  {-argName "-methodtype" -nrargs 1 -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-callprotection" -nrargs 1 -type "all|protected|public" -default all}
  {-argName "-application"}
  {-argName "-nomixins"}
  {-argName "-incontext"}
  {-argName "pattern" -required 0}
}
objectInfoMethod children NsfObjInfoChildrenMethod {
  {-argName "pattern" -required 0}
}
objectInfoMethod class NsfObjInfoClassMethod {
}
objectInfoMethod filterguard NsfObjInfoFilterguardMethod {
  {-argName "filter" -required 1}
}
objectInfoMethod filtermethods NsfObjInfoFiltermethodsMethod {
  {-argName "-guards"}
  {-argName "-order"}
  {-argName "pattern"}
}
objectInfoMethod forward NsfObjInfoForwardMethod {
  {-argName "-definition"}
  {-argName "name"}
}
objectInfoMethod hasmixin NsfObjInfoHasMixinMethod {
  {-argName "class" -type class}
}
objectInfoMethod hasnamespace NsfObjInfoHasnamespaceMethod {
}
objectInfoMethod hastype NsfObjInfoHasTypeMethod {
  {-argName "class" -type class}
}
objectInfoMethod is NsfObjInfoIsMethod {
  {-argName "objectkind" -type "class|baseclass|metaclass"}
}
objectInfoMethod method NsfObjInfoMethodMethod {
  {-argName "infomethodsubcmd" -type "args|body|definition|handle|parameter|parametersyntax|type|precondition|postcondition"}
  {-argName "name"}
}
objectInfoMethod methods NsfObjInfoMethodsMethod {
  {-argName "-methodtype" -nrargs 1 -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-callprotection" -nrargs 1 -type "all|protected|public" -default public}
  {-argName "-nomixins"}
  {-argName "-incontext"}
  {-argName "pattern"}
}

objectInfoMethod mixinclasses NsfObjInfoMixinclassesMethod {
  {-argName "-guards"}
  {-argName "-order"}
  {-argName "pattern" -type objpattern}
}
objectInfoMethod mixinguard NsfObjInfoMixinguardMethod {
  {-argName "mixin"  -required 1}
}
objectInfoMethod parent NsfObjInfoParentMethod {
}
objectInfoMethod precedence NsfObjInfoPrecedenceMethod {
  {-argName "-intrinsic"}
  {-argName "pattern" -required 0}
}
objectInfoMethod slotobjects NsfObjInfoSlotObjectsMethod {
  {-argName "pattern" -required 0}
}
objectInfoMethod vars NsfObjInfoVarsMethod {
  {-argName "pattern" -required 0}
}

#
# info class methods
#
classInfoMethod filterguard NsfClassInfoFilterguardMethod {
  {-argName "filter" -required 1}
}
classInfoMethod filtermethods NsfClassInfoFiltermethodsMethod {
  {-argName "-guards"}
  {-argName "pattern"}
}
classInfoMethod forward NsfClassInfoForwardMethod {
  {-argName "-definition"}
  {-argName "name"}
}
classInfoMethod heritage NsfClassInfoHeritageMethod {
  {-argName "pattern"}
}
classInfoMethod instances NsfClassInfoInstancesMethod {
  {-argName "-closure"}
  {-argName "pattern" -type objpattern}
}

classInfoMethod method NsfClassInfoMethodMethod {
  {-argName "infomethodsubcmd" -type "args|body|definition|handle|parameter|parametersyntax|type|precondition|postcondition"}
  {-argName "name"}
}
classInfoMethod methods NsfClassInfoMethodsMethod {
  {-argName "-methodtype" -nrargs 1 -type "all|scripted|builtin|alias|forwarder|object|setter"}
  {-argName "-callprotection" -nrargs 1 -type "all|protected|public" -default public}
  {-argName "-nomixins"}
  {-argName "-incontext"}
  {-argName "pattern"}
}
classInfoMethod mixinclasses NsfClassInfoMixinclassesMethod {
  {-argName "-closure"}
  {-argName "-guards"}
  {-argName "pattern" -type objpattern}
}
classInfoMethod mixinguard NsfClassInfoMixinguardMethod {
  {-argName "mixin"  -required 1}
}
classInfoMethod mixinof  NsfClassInfoMixinOfMethod {
  {-argName "-closure"}
  {-argName "-scope" -required 0 -nrargs 1 -type "all|class|object"}
  {-argName "pattern" -type objpattern}
}
classInfoMethod slots NsfClassInfoSlotsMethod {
}
classInfoMethod subclass NsfClassInfoSubclassMethod {
  {-argName "-closure"}
  {-argName "pattern" -type objpattern}
}
classInfoMethod superclass NsfClassInfoSuperclassMethod {
  {-argName "-closure"}
  {-argName "pattern" -type tclobj}
}

#
# class methods
#

classMethod alloc NsfCAllocMethod {
  {-argName "name" -required 1 -type tclobj}
}

classMethod create NsfCCreateMethod {
  {-argName "name" -required 1}
  {-argName "args" -type allargs}
}

classMethod dealloc NsfCDeallocMethod {
  {-argName "object" -required 1 -type tclobj}
}

classMethod new NsfCNewMethod {
  {-argName "-childof" -type object -nrargs 1}
  {-argName "args" -required 0 -type args}
}
classMethod filterguard NsfCFilterGuardMethod {
  {-argName "filter" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}
classMethod mixinguard NsfCMixinGuardMethod {
  {-argName "mixin" -required 1}
  {-argName "guard" -required 1 -type tclobj}
}

classMethod recreate NsfCRecreateMethod {
  {-argName "name" -required 1 -type tclobj}
  {-argName "args" -type allargs}
}
#
# check methods
#
# checkMethod required NsfCheckRequiredArgs {
#   {-argName "name" -required 1}
#   {-argName "value" -required 0 -type tclobj}
# }
# checkMethod boolean NsfCheckBooleanArgs {
#   {-argName "name" -required 1}
#   {-argName "value" -required 0 -type tclobj}
# }
