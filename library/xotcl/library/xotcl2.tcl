package provide XOTcl 2.0
package require nx

#######################################################
# Classical ::xotcl*
#######################################################
namespace eval ::xotcl {
  #
  # Set XOTcl version variables
  #
  set ::xotcl::version 2.0
  set ::xotcl::patchlevel .0

  #
  # Perform the basic setup of XOTcl. First, let us allocate the
  # basic classes of XOTcl. This call creates the classes
  # ::xotcl::Object and ::xotcl::Class and defines these as root class
  # of the object system and as root meta class.
  #
  ::nsf::createobjectsystem ::xotcl::Object ::xotcl::Class {
    -class.alloc alloc 
    -class.create create
    -class.dealloc dealloc
    -class.recreate recreate 
    -class.requireobject __unknown 
    -object.configure configure 
    -object.cleanup cleanup
    -object.defaultmethod defaultmethod 
    -object.destroy destroy 
    -object.init init 
    -object.move move 
    -object.objectparameter objectparameter 
    -object.residualargs residualargs
    -object.unknown unknown
  }

  #
  # create ::nx and ::nsf namespaces, otherwise mk_pkgindex will fail
  #
  namespace eval ::nx {}
  namespace eval ::nsf {}

  #
  # get frequenly used primitiva into the ::xotcl namespace
  #
  namespace import ::nsf::configure ::nsf::my ::nsf::next ::nsf::finalize ::nsf::interp
  namespace import ::nsf::alias ::nsf::is ::nsf::relation

  #namespace import ::nx::Attribute
  # if we do this, "::xotcl::Class create Role -superclass Attribute"  will fail.
  #interp alias {} ::xotcl::Attribute {} ::nx::Attribute
  ::nx::Class create ::xotcl::Attribute -superclass ::nx::Attribute

  proc ::xotcl::self {{arg "object"}} {
      switch $arg {
	  next {
	      set handle [uplevel ::nsf::current $arg]
	      method_handle_to_xotcl $handle
	  }
	  default {uplevel ::nsf::current $arg}
      }
  }

  # provide the standard command set for ::xotcl::Object
  foreach cmd [info command ::nsf::cmd::Object::*] {
    set cmdName [namespace tail $cmd]
    if {$cmdName in [list "setter"]} continue
    ::nsf::alias Object $cmdName $cmd
  }

  # provide some Tcl-commands as methods for ::xotcl::Object
  foreach cmd {array append eval incr lappend set subst unset trace} {
    ::nsf::alias Object $cmd -objscope ::$cmd
  }

  # provide the standard command set for ::xotcl::Class
  foreach cmd [info command ::nsf::cmd::Class::*] {
    set cmdName [namespace tail $cmd]
    if {$cmdName in [list "setter"]} continue
    ::nsf::alias Class $cmdName $cmd
  }

  # protect some methods against redefinition
  ::nsf::methodproperty Object destroy redefine-protected true
  ::nsf::methodproperty Class  alloc   redefine-protected true
  ::nsf::methodproperty Class  dealloc redefine-protected true
  ::nsf::methodproperty Class  create  redefine-protected true

  # define instproc and proc
  ::nsf::method Class instproc {
    name arguments body precondition:optional postcondition:optional
  } {
    set conditions [list]
    if {[info exists precondition]}  {lappend conditions -precondition  $precondition}
    if {[info exists postcondition]} {lappend conditions -postcondition $postcondition}
    ::nsf::method [self] $name $arguments $body {*}$conditions
  }

  ::nsf::method Object proc {
    name arguments body precondition:optional postcondition:optional
  } {
    set conditions [list]
    if {[info exists precondition]}  {lappend conditions -precondition  $precondition}
    if {[info exists postcondition]} {lappend conditions -postcondition $postcondition}
    ::nsf::method [self] -per-object $name $arguments $body {*}$conditions
  }

  # define - like in XOTcl 1 - a minimal implementation of "method"
  Object instproc method {name arguments body} {
    :proc $name $arguments $body                                  
  }
  Class instproc method {-per-object:switch name arguments body} {
    if {${per-object}} {
      :proc $name $arguments $body       
    } else {
      :instproc $name $arguments $body  
    }
  }

  # define forward methods
  ::nsf::forward Object forward ::nsf::forward %self -per-object
  ::nsf::forward Class instforward ::nsf::forward %self

  Class instproc unknown {args} {
    #puts stderr "use '[self] create $args', not '[self] $args'"
    uplevel [list [self] create {*}$args]
  }

  Object instproc unknown {m args} {
    if {![self isnext]} {
      error "[self]: unable to dispatch method '$m'"
    }
  }

  # "init" must exist on Object. per default it is empty.
  Object instproc init args {}

  Object instproc self {} {::xotcl::self}

  #
  # object-parameter definition, backwards compatible
  #
  ::xotcl::Object instproc objectparameter {} {
    set parameterdefinitions [::nsf::parametersFromSlots [self]]
    lappend parameterdefinitions args
    #puts stderr "*** parameter definition for [self]: $parameterdefinitions"
    return $parameterdefinitions
  }

  #
  # Use parameter definition from next 
  # (same with classInfo parameter, see below)
  ::nsf::alias ::xotcl::Class parameter ::nsf::classes::nx::Class::parameter

  # We provide a default value for superclass (when no superclass is
  # specified explicitely) and metaclass, in case they should differ
  # from the root classes of the object system.

  ::xotcl::Class parameter {
    {__default_superclass ::xotcl::Object}
    {__default_metaclass ::xotcl::Class}
  }

  ############################################
  # system slots
  ############################################
  proc register_system_slots {os} {
    # We need explicit ::xotcl prefixes, since they are always skipped
    # if not specified
    ${os}::Object alloc ${os}::Class::slot
    ${os}::Object alloc ${os}::Object::slot
    
    ::nx::RelationSlot create ${os}::Class::slot::superclass
    ::nsf::alias         ${os}::Class::slot::superclass assign ::nsf::relation
    ::nx::RelationSlot create ${os}::Object::slot::class -multivalued false
    ::nsf::alias         ${os}::Object::slot::class assign ::nsf::relation

    ::nx::RelationSlot create ${os}::Object::slot::mixin \
        -methodname object-mixin
    ::nx::RelationSlot create ${os}::Object::slot::filter \
        -methodname object-filter \
        -elementtype ""

    ::nx::RelationSlot create ${os}::Class::slot::instmixin \
        -methodname class-mixin
    ::nx::RelationSlot create ${os}::Class::slot::instfilter \
        -methodname class-filter \
        -elementtype ""
  }
  register_system_slots ::xotcl
  proc ::xotcl::register_system_slots {} {}

  ########################
  # Info definition
  ########################
  Object create ::xotcl::objectInfo
  Object create ::xotcl::classInfo

  # note, we are using ::xotcl::infoError defined earlier
  Object instforward info -onerror ::nsf::infoError ::xotcl::objectInfo %1 {%@2 %self}
  Class  instforward info -onerror ::nsf::infoError ::xotcl::classInfo %1 {%@2 %self}

  objectInfo proc info {obj} {
    set methods [list]
    foreach m [::info commands ::xotcl::objectInfo::*] {
      set name [namespace tail $m]
      if {$name eq "unknown"} continue
      lappend methods $name
    }
    return "valid options are: [join [lsort $methods] {, }]"
  }
  objectInfo proc unknown {method args} {
    error "[::xotcl::self] unknown info option \"$method\"; [:info info]"
  }

  classInfo proc info {cl} {
    set methods [list]
    foreach m [::info commands ::xotcl::classInfo::*] {
      set name [namespace tail $m]
      if {$name eq "unknown"} continue
      lappend methods $name
    }
    return "valid options are: [join [lsort $methods] {, }]"
  }

  classInfo proc unknown {method args} {
    error "[::xotcl::self] unknown info option \"$method\"; [:info info]"
  }

  #
  # Backward compatibility info subcommands;
  #
  # TODO: should go finally into a library.
  #
  # Obsolete methods
  #
  #   already emulated:
  #
  #    => info -per-object method parameter .... replaces
  #     info instargs
  #     info instnonposargs
  #     info instdefault
  #
  #    => info method .... replaces
  #     info body
  #     info instbody
  #
  #    => info methods .... replaces
  #     info commands
  #     info instcommands
  #     info procs
  #     info instprocs
  #     info parametercmd
  #     info instparametercmd
  #
  #    => info is (resp. ::xotcl::is) replaces
  #     info isobject
  #     info isclass
  #     info ismetaclass
  #     info ismixin
  #     info istype
  #
  #    => info method .... replaces
  #     proc
  #     instproc
  #     info args
  #     info nonposargs
  #     info default
  #
  # TODO mark all absolete calls at least as deprecated in library
  #

  proc ::xotcl::info_args {allocation o method} {
    set result [list]
    foreach \
        argName [::nsf::cmd::${allocation}Info::method $o args $method] \
        flag    [::nsf::cmd::${allocation}Info::method $o parameter $method] {
          if {[string match -* $flag]} continue
          lappend result $argName
        }
    #puts stderr "+++ get ${inst}args for $o $method => $result"
    return $result
  }

  proc ::xotcl::info_nonposargs {allocation o method} {
    set result [list]
    foreach flag [::nsf::cmd::${allocation}Info::method $o parameter $method] {
      if {![string match -* $flag]} continue
      lappend result $flag
    }
    #puts stderr "+++ get ${inst}nonposargs for $o $method => $result"
    return $result
  }
  proc ::xotcl::info_default {allocation o method arg varName} {
    foreach \
        argName [::nsf::cmd::${allocation}Info::method $o args $method] \
        flag    [::nsf::cmd::${allocation}Info::method $o parameter $method] {
          if {$argName eq $arg} {
            upvar 3 $varName default
            if {[llength $flag] == 2} {
              set default [lindex $flag 1]
              #puts stderr "--- get ${inst}default for $o $method $arg => $default"
              return 1
            }
            #puts stderr "--- get ${inst}default for $o $method $arg fails"
            set default ""
            return 0
          }
        }
    error "procedure \"$method\" doesn't have an argument \"$varName\""
  }

  classInfo eval {
    :proc instargs {o method} {::xotcl::info_args Class $o $method}
    :proc args     {o method} {::xotcl::info_args Object $o $method}
    :proc instnonposargs {o method} {::xotcl::info_nonposargs Class $o $method}
    :proc nonposargs     {o method} {::xotcl::info_nonposargs Object $o $method}
    :proc instdefault {o method arg var} {::xotcl::info_default Class $o $method $arg $var}
    :proc default     {o method arg var} {::xotcl::info_default Object $o $method $arg $var}

    # info options emulated by "info method ..."
    :proc instbody    {o methodName} {::nsf::cmd::ClassInfo::method $o body $methodName}
    :proc instpre     {o methodName} {::nsf::cmd::ClassInfo::method $o precondition  $methodName}
    :proc instpost    {o methodName} {::nsf::cmd::ClassInfo::method $o postcondition $methodName}

    # info options emulated by "info methods"
    :proc instcommands {o {pattern:optional ""}} {
      ::nsf::cmd::ClassInfo::methods $o {*}$pattern
    }
    :proc instprocs   {o {pattern:optional ""}} {
      ::nsf::cmd::ClassInfo::methods $o -methodtype scripted {*}$pattern
    }
    :proc parametercmd {o {pattern:optional ""}} {
      ::nsf::cmd::ClassInfo::methods $o -per-object -methodtype setter {*}$pattern
    }
    :proc instparametercmd {o {pattern:optional ""}} {
      ::nsf::cmd::ClassInfo::methods $o -methodtype setter {*}$pattern
    }
    # assertion handling
    :proc instinvar   {o} {::nsf::assertion $o class-invar}
  }

  objectInfo eval {
    :proc args        {o method} {::xotcl::info_args Object $o $method}
    :proc nonposargs  {o method} {::xotcl::info_nonposargs Object $o $method}
    :proc default     {o method arg var} {::xotcl::info_default Object $o $method $arg $var}

    # info options emulated by "info method ..."
    :proc body        {o methodName} {::nsf::cmd::ObjectInfo::method $o body $methodName}
    :proc pre         {o methodName} {::nsf::cmd::ObjectInfo::method $o pre $methodName}
    :proc post        {o methodName} {::nsf::cmd::ObjectInfo::method $o post $methodName}

    # info options emulated by "info methods"
    :proc commands    {o {pattern:optional ""}} {
      ::nsf::cmd::ObjectInfo::methods $o {*}$pattern
    }
    :proc procs       {o {pattern:optional ""}} {
      ::nsf::cmd::ObjectInfo::methods $o -methodtype scripted {*}$pattern
    }
    :proc methods {
      o -nocmds:switch -noprocs:switch -incontext:switch pattern:optional
    } {
      set methodtype all
      if {$nocmds} {set methodtype scripted}
      if {$noprocs} {if {$nocmds} {return ""}; set methodtype builtin}
      set cmd [list ::nsf::cmd::ObjectInfo::callable $o methods -methodtype $methodtype]
      if {$incontext} {lappend cmd -incontext}
      if {[info exists pattern]} {lappend cmd $pattern}
      eval $cmd
    }
    # object filter mapping
    :proc filter {o -order:switch -guards:switch pattern:optional} {
      set guardsFlag [expr {$guards ? "-guards" : ""}]
      set patternArg [expr {[info exists pattern] ? [list $pattern] : ""}]
      if {$order && !$guards} {
        set def [::nsf::cmd::ObjectInfo::filter $o -order {*}$guardsFlag {*}$patternArg]
        set def [method_handles_to_xotcl $def]
      } else {
        set def [::nsf::cmd::ObjectInfo::filter $o {*}$guardsFlag {*}$patternArg]
      }
      #puts stderr "  => $def"
      return $def
    }
    # assertion handling
    :proc check {o} {
      ::xotcl::checkoption_internal_to_xotcl1 [::nsf::assertion $o check]
    }
    :proc invar {o} {::nsf::assertion $o object-invar}
  }

  foreach cmd [::info command ::nsf::cmd::ObjectInfo::*] {
    set cmdName [namespace tail $cmd]
    if {$cmdName in [list "callable" "filter" "method" "methods"]} continue
    ::nsf::alias ::xotcl::objectInfo $cmdName $cmd
    ::nsf::alias ::xotcl::classInfo $cmdName $cmd
  }

  foreach cmd [::info command ::nsf::cmd::ClassInfo::*] {
    set cmdName [namespace tail $cmd]
    if {$cmdName in [list "forward" "method" "methods" \
                         "mixinof" "object-mixin-of" \
                         "filter" "filterguard" \
                         "mixin" "mixinguard"]} continue
    ::nsf::alias ::xotcl::classInfo $cmdName $cmd
  }

  ::nsf::alias ::xotcl::objectInfo is ::nsf::objectproperty
  ::nsf::alias ::xotcl::classInfo is ::nsf::objectproperty
  ::nsf::alias ::xotcl::classInfo classparent ::nsf::cmd::ObjectInfo::parent
  ::nsf::alias ::xotcl::classInfo classchildren ::nsf::cmd::ObjectInfo::children
  ::nsf::alias ::xotcl::classInfo instmixin ::nsf::cmd::ClassInfo::mixin
  ::nsf::alias ::xotcl::classInfo instmixinguard ::nsf::cmd::ClassInfo::mixinguard
  #::nsf::alias ::xotcl::classInfo instmixinof ::nsf::cmd::ClassInfo::class-mixin-of
  ::nsf::forward ::xotcl::classInfo instmixinof ::nsf::cmd::ClassInfo::mixinof %1 -scope class
  ::nsf::alias ::xotcl::classInfo instfilter ::nsf::cmd::ClassInfo::filter
  ::nsf::alias ::xotcl::classInfo instfilterguard ::nsf::cmd::ClassInfo::filterguard
  ::nsf::alias ::xotcl::classInfo instforward ::nsf::cmd::ClassInfo::forward
  #::nsf::alias ::xotcl::classInfo mixinof ::nsf::cmd::ClassInfo::object-mixin-of
  ::nsf::forward ::xotcl::classInfo mixinof ::nsf::cmd::ClassInfo::mixinof %1 -scope object
  ::nsf::alias ::xotcl::classInfo parameter ::nx::classInfo::parameter

  # assertion handling
  ::nsf::alias ::xotcl::classInfo invar objectInfo::invar
  ::nsf::alias ::xotcl::classInfo check objectInfo::check

  # define info methods from objectInfo on classInfo as well
  ::nsf::alias classInfo body     objectInfo::body
  ::nsf::alias classInfo commands objectInfo::commands
  ::nsf::alias classInfo filter   objectInfo::filter
  ::nsf::alias classInfo methods  objectInfo::methods
  ::nsf::alias classInfo procs    objectInfo::procs
  ::nsf::alias classInfo pre      objectInfo::pre
  ::nsf::alias classInfo post     objectInfo::post

  # emulation of isobject, isclass ...
  Object instproc isobject    {{object:substdefault "[self]"}} {::nsf::objectproperty $object object}
  Object instproc isclass     {{class:substdefault  "[self]"}} {::nsf::objectproperty $class class}
  Object instproc ismetaclass {{class:substdefault  "[self]"}} {::nsf::objectproperty $class metaclass}
  Object instproc ismixin     {class}  {::nsf::is [self] object -hasmixin $class}
  Object instproc istype      {class}  {::nsf::is [self] type $class}

  ::nsf::alias Object contains ::nsf::classes::nx::Object::contains
  ::xotcl::Class instforward slots %self contains \
      -object {%::nsf::dispatch [::xotcl::self] -objscope ::subst [::xotcl::self]::slot}
  #
  # define parametercmd and instparametercmd in terms of ::nx method setter
  # define filterguard and instfilterguard in terms of filterguard
  # define mixinguard and instmixinguard in terms of mixinguard
  #
  ::nsf::alias Object parametercmd    ::nsf::classes::nx::Object::setter
  ::nsf::alias Class instparametercmd ::nsf::classes::nx::Class::setter

  ::nsf::alias Class filterguard      ::nsf::cmd::Object::filterguard
  ::nsf::alias Class instfilterguard  ::nsf::cmd::Class::filterguard

  ::nsf::alias Class mixinguard       ::nsf::cmd::Object::mixinguard
  ::nsf::alias Class instmixinguard   ::nsf::cmd::Class::mixinguard

  # assertion handling
  proc checkoption_xotcl1_to_internal checkoptions {
    set options [list]
    foreach option $checkoptions {
      if {$option eq "invar"} {
        lappend options "object-invar"
      } elseif {$option eq "instinvar"} {
        lappend options "class-invar"
      } else {
        lappend options $option
      }
    }
    return $options
  }
  proc checkoption_internal_to_xotcl1 checkoptions {
    set options [list]
    foreach option $checkoptions {
      if {$option eq "object-invar"} {
        lappend options "invar"
      } elseif {$option eq "class-invar"} {
        lappend options "instinvar"
      } else {
        lappend options $option
      }
    }
    return $options
  }
  proc method_handles_to_xotcl definitions {
    set defs [list]
    foreach def $definitions {lappend defs [method_handle_to_xotcl $def]}
    return $defs
  }
  proc method_handle_to_xotcl methodHandle {
    set definition [::nx::Object info method definition $methodHandle]
    #puts "method_handle_to_xotcl raw definition '$methodHandle' // $definition"
    if {$definition ne ""} {
      set obj [lindex $definition 0]
      set modifier [lindex $definition 1]
	if {$modifier eq "object"} {
        set prefix ""
        set kind [lindex $definition 2]
        set name [lindex $definition 3]
      } else {
	set prefix [expr {[::nsf::objectproperty $obj class] ? "inst" : ""}]
        set kind $modifier
        set name [lindex $definition 2]
      }
      if {$kind eq "method"} {
        set kind proc
      } elseif {$kind eq "setter"} {
        set kind parametercmd
      } elseif {$kind eq "alias"} {
	set kind "cmd"
	set name [lindex $definition end-1]
      }
      set definition [list [lindex $definition 0] ${prefix}$kind $name]
    }
    #puts "method_handle_to_xotcl gets handle '$methodHandle' // $definition"
    return $definition
  }
 

  Object instproc check {checkoptions} {
    ::nsf::assertion [self] check [::xotcl::checkoption_xotcl1_to_internal $checkoptions]
  }
  Object instforward invar     ::nsf::assertion %self object-invar
  Class  instforward instinvar ::nsf::assertion %self class-invar

  Object instproc abstract {methtype methname arglist} {
    if {$methtype ne "proc" && $methtype ne "instproc" && $methtype ne "method"} {
      error "invalid method type '$methtype', \
	must be either 'proc', 'instproc' or 'method'."
    }
    :$methtype $methname $arglist "
      if {!\[::xotcl::self isnextcall\]} {
        error \"Abstract method $methname $arglist called\"
      } else {::xotcl::next}
    "
  }

  # support for XOTcl specific convenience routines
  Object instproc hasclass cl {
    if {[::nsf::is [self] object -hasmixin $cl]} {return 1}
    ::nsf::is [self] type $cl
  }
  Object instproc filtersearch {filter} {
    set handle [::nsf::cmd::ObjectInfo::method [self] filter $filter]
    return [method_handle_to_xotcl $handle]
  }
  Object instproc procsearch {name} {
    set handle [::nsf::cmd::ObjectInfo::callable [self] method $name]
    return [method_handle_to_xotcl $handle]

    if {$definition ne ""} {
      foreach {obj modifier kind} $definition break
      if {$modifier ne "object"} {
        set kind $modifier
        set perClass [::nsf::is $obj class]
      } else {
        set perClass 0
      }
      switch $kind {
        alias   {if {$perClass} {set kind "instcmd"} else {set kind "cmd"}}
        forward {if {$perClass} {set kind "instforward"}}
        method  {if {$perClass} {set kind "instproc"} else {set kind "proc"}}
        setter  {if {$perClass} {set kind "instparametercmd"} else {set kind "parametercmd"}}
        default {error "not handeled: $definition"}
      }
      #puts stderr "return: [list $obj $kind $name]"
      return [list $obj $kind $name]
    }
  }
  Class instproc allinstances {} {
    # TODO: mark it deprecated
    return [:info instances -closure]
  }

  # keep old object interface for XOTcl
  Object proc unsetExitHandler {} {::nsf::unsetExitHandler $newbody}
  Object proc setExitHandler   {newbody} {::nsf::setExitHandler $newbody}
  Object proc getExitHandler   {} {::nsf::getExitHandler}

  # resue some definitions from next scripting
  ::nsf::alias ::xotcl::Object copy ::nsf::classes::nx::Object::copy
  ::nsf::alias ::xotcl::Object move ::nsf::classes::nx::Object::move
  ::nsf::alias ::xotcl::Object defaultmethod ::nsf::classes::nx::Object::defaultmethod

  ::nsf::alias ::xotcl::Class -per-object __unknown ::nx::Class::__unknown

  proc myproc {args} {linsert $args 0 [::xotcl::self]}
  proc myvar  {var}  {.requireNamespace; return [::xotcl::self]::$var}

  Object create ::xotcl::config
  config proc load {obj file} {
    source $file
    foreach i [array names ::auto_index [list $obj *proc *]] {
      set type [lindex $i 1]
      set meth [lindex $i 2]
      if {[$obj info ${type}s $meth] == {}} {
        $obj $type $meth auto $::auto_index($i)
      }
  }
  }
  
  config proc mkindex {meta dir args} {
    set sp {[ 	]+}
    set st {^[ 	]*}
    set wd {([^ 	;]+)}
    foreach creator $meta {
      ::lappend cp $st$creator${sp}create$sp$wd
      ::lappend ap $st$creator$sp$wd
    }
    foreach methodkind {proc instproc} {
      ::lappend mp $st$wd${sp}($methodkind)$sp$wd
    }
    foreach cl [concat ::xotcl::Class [::xotcl::Class info heritage]] {
      ::lappend meths {*}[$cl info instcommands]
    }
    set old [pwd]
    cd $dir
    ::append idx "# Tcl autoload index file, version 2.0\n"
    ::append idx "# xotcl additions generated with "
    ::append idx "\"::xotcl::config::mkindex [list $meta] [list $dir] $args\"\n"
    set oc 0
    set mc 0
    foreach file [glob -nocomplain -- {*}$args] {
      if {[catch {set f [open $file]} msg]} then {
        catch {close $f}
        cd $old
        error $msg
      }
      while {[gets $f line] >= 0} {
        foreach c $cp {
          if {[regexp $c $line x obj]==1 &&
              [string index $obj 0]!={$}} then {
            ::incr oc
            ::append idx "set auto_index($obj) "
            ::append idx "\"::xotcl::config::load $obj \$dir/$file\"\n"
          }
        }
        foreach a $ap {
          if {[regexp $a $line x obj]==1 &&
              [string index $obj 0]!={$} &&
              [lsearch -exact $meths $obj]==-1} {
            ::incr oc
            ::append idx "set auto_index($obj) "
            ::append idx "\"::xotcl::config::load $obj \$dir/$file\"\n"
          }
        }
        foreach m $mp {
          if {[regexp $m $line x obj ty pr]==1 &&
              [string index $obj 0]!={$} &&
              [string index $pr 0]!={$}} then {
            ::incr mc
            ::append idx "set \{auto_index($obj "
            ::append idx "$ty $pr)\} \"source \$dir/$file\"\n"
          }
        }
      }
      close $f
    }
    set t [open tclIndex a+]
    puts $t $idx nonewline
    close $t
    cd $old
    return "$oc objects, $mc methods"
  }

  #
  # if cutTheArg not 0, it cut from upvar argsList
  #
  Object instproc extractConfigureArg {al name {cutTheArg 0}} {
    set value ""
    upvar $al argList
    set largs [llength $argList]
    for {set i 0} {$i < $largs} {incr i} {
      if {[lindex $argList $i] == $name && $i + 1 < $largs} {
        set startIndex $i
        set endIndex [expr {$i + 1}]
        while {$endIndex < $largs &&
               [string first - [lindex $argList $endIndex]] != 0} {
          lappend value [lindex $argList $endIndex]
          incr endIndex
        }
      }
    }
    if {[info exists startIndex] && $cutTheArg != 0} {
      set argList [lreplace $argList $startIndex [expr {$endIndex - 1}]]
    }
    return $value
  }

  Object create ::xotcl::rcs
  rcs proc date string {
    lreplace [lreplace $string 0 0] end end
  }
  rcs proc version string {
    lindex $string 2
  }

  #
  # package support
  #
  # puts this for the time being into XOTcl
  #
  ::xotcl::Class instproc uses list {
    foreach package $list {
      ::xotcl::package import -into [::xotcl::self] $package
      puts stderr "*** using ${package}::* in [::xotcl::self]"
    }
  }

  ::nx::Class create ::xotcl::package -superclass ::xotcl::Class -parameter {
    provide
    {version 1.0}
    {autoexport {}}
    {export {}}
  } {
    
    :public object method create {name args} {
      set nq [namespace qualifiers $name]
      if {$nq ne "" && ![namespace exists $nq]} {Object create $nq}
      next
    }

    :public object method extend {name args} {
      :require $name
	$name configure {*}$args
    }
    
    :public object method contains script {
      if {[info exists :provide]} {
        package provide [set :provide] [set :version]
      } else {
        package provide [::xotcl::self] [set :version]
      }
      namespace eval [::xotcl::self] {namespace import ::xotcl::*}
      namespace eval [::xotcl::self] $script
      foreach e [set :export] {
        set nq [namespace qualifiers $e]
        if {$nq ne ""} {
          namespace eval [::xotcl::self]::$nq [list namespace export [namespace tail $e]]
        } else {
          namespace eval [::xotcl::self] [list namespace export $e]
        }
      }
      foreach e [set :autoexport] {
        namespace eval :: [list namespace import [::xotcl::self]::$e]
      }
    }
    
    :public object method unknown args {
      #puts stderr "unknown: package $args"
      [set :packagecmd] {*}$args
    }
    
    :public object method verbose value {
      set :verbose $value
    }
    
    :public object method present args {
      if {$::tcl_version<8.3} {
        switch -exact -- [lindex $args 0] {
          -exact  {set pkg [lindex $args 1]}
          default {set pkg [lindex $args 0]}
        }
        if {[info exists :loaded($pkg)]} {
          return ${:loaded}($pkg)
        } else {
          error "not found"
        }
      } else {
	[set :packagecmd] present {*}$args
      }
    }
    
    :public object method import {{-into ::} pkg} {
      :require $pkg
      namespace eval $into [subst -nocommands {
        #puts stderr "*** package import ${pkg}::* into [namespace current]"
        namespace import ${pkg}::*
      }]
      # import subclasses if any
      foreach e [$pkg export] {
        set nq [namespace qualifiers $e]
        if {$nq ne ""} {
          namespace eval $into$nq [list namespace import ${pkg}::$e]
        }
      }
    }
    
    :public object method require args {
      #puts "XOTCL package require $args, current=[namespace current]"
      set prevComponent ${:component}
      if {[catch {set v [package present {*}$args]} msg]} {
        #puts stderr "we have to load $msg"
        switch -exact -- [lindex $args 0] {
          -exact  {set pkg [lindex $args 1]}
          default {set pkg [lindex $args 0]}
        }
        set :component $pkg
        lappend :uses($prevComponent) ${:component}
        set v [uplevel \#1 [set :packagecmd] require $args]
        if {$v ne "" && ${:verbose}} {
        set path [lindex [::package ifneeded $pkg $v] 1]
          puts "... $pkg $v loaded from '$path'"
          set :loaded($pkg) $v   ;# loaded stuff needed for Tcl 8.0
        }
      }
      set :component $prevComponent
      return $v
    }
    
    set :component .
    set :verbose 0
    set :packagecmd ::package
  }
  
  if {[info exists cmd]} {unset cmd}

  # Documentation stub object -> just ignore per default.
  # if xoDoc is loaded, documentation will be activated
  ::xotcl::Object create ::xotcl::@
  ::xotcl::@ proc unknown args {}
  
  set ::xotcl::confdir ~/.xotcl
  set ::xotcl::logdir $::xotcl::confdir/log
  namespace import ::nsf::tmpdir

  # finally, export contents defined for XOTcl
  namespace export Object Class Attribute myproc myvar my self next @
}

foreach ns {::nsf ::nx ::xotcl} {
  puts stderr "$ns exports [namespace eval $ns {lsort [namespace export]}]"
}