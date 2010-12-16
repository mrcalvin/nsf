package provide nx::test 1.0
package require nx

namespace eval ::nx::test {
  namespace import ::nx::*

  # @file Simple regression test support.

  Class create Test {
    #
    # Class Test is used to configure test instances, which can 
    # be configured by the following parameters:
    #
    # @param cmd the command to be executed
    # @param expected  the expected result
    # @param count  number of executions of cmd
    # @param pre a command to be executed at the begin of the test (before cmd)
    # @param post a command to be executed after the test (after all cmds)
    # @param namespace in which pre, post and cmd are evaluated; default "::"
    #
    # The defined tests can be executed by [:cmd "Test run"]

    :attribute {name ""}
    :attribute cmd 
    :attribute {namespace ::}
    :attribute {verbose 0} 
    :attribute {expected 1} 
    :attribute {count 100} 
    :attribute msg 
    :attribute setResult 
    :attribute errorReport
    :attribute pre 
    :attribute post

    set :count 0

    :public class-object method case {name arg:optional} {
      #
      # Experimental version of Test case, which (1) accepts test case as argument
      # and (2) destroys all created objects on exit (auto cleanup)
      #
      # General limitation: namespace resolving differs in nested evals
      # from global evals. So, this approach is not suitable for all test
      # (but for most).
      #
      # Current limitations: just for nx::Objects, no method/mixin cleanup/var cleanup
      #
      set :case $name
      if {[info exists arg]} {
        foreach o [Object info instances -closure] {set pre_exist($o) 1}
        namespace eval :: [list [current] eval $arg]
        #:eval $arg
        foreach o [Object info instances -closure] {
          if {[info exists pre_exist($o)]} continue
          #puts "must destroy $o"
          if {[::nsf::isobject $o]} {$o destroy}
        }
      }
    }

    :public class-object method parameter {name value:optional} {
      if {[info exists value]} {
        #[[current] slot $name] default $value
	#:slot $name default $value
	[self]::slot::$name default $value
        ::nsf::invalidateobjectparameter [self]
      } else {
	return [[self]::slot::$name $name default]
      }
    }

    :public class-object method new args {
      set testfile [file rootname [file tail [info script]]]
      if {[info exists :case]} {
	if {![info exists :ccount(${:case})]} {set :ccount(${:case}) 0}
	set :name $testfile/${:case}.[format %.3d [incr :ccount(${:case})]]
      } else {
	set :name $testfile/t.[format %.3d [incr :count]]
      }
      :create ${:name} -name ${:name} {*}$args
    }

    :public class-object method run {} {
      set startTime [clock clicks -milliseconds]
      foreach example [lsort [:info instances -closure]] {
	$example run
      }
      puts stderr "Total Time: [expr {[clock clicks -milliseconds]-$startTime}] ms"
    }
    
    :public method call {msg cmd} {
      if {[:verbose]} {puts stderr "$msg: $cmd"}
      #if {[catch {::namespace eval ${:namespace} $cmd} result]} {
        #puts stderr ERROR=$result
      #}
      #puts stderr "$msg: $cmd => $result"
      #return $result
      return [::namespace eval ${:namespace} $cmd]
    }
   
    :public method run args {
      if {[info exists :pre]} {:call "pre" ${:pre}}
      if {![info exists :msg]} {set :msg ${:cmd}}
      set gotError [catch {:call "run" ${:cmd}} r]
      if {[info exists :setResult]} {set r [eval [set :setResult]]}
      if {$r eq ${:expected}} {
        if {$gotError} {
          set c 1
        } else {
          if {[info exists :count]} {set c ${:count}} {set c 1000}
        }
	if {[:verbose]} {
	  puts stderr "running test $c times"
	}
	if {$c > 1} {
	  #set r0 [time ${:cmd} $c]
	  #puts stderr "time {time ${:cmd} $c}"
	  set r1 [time {time {::namespace eval ${:namespace} ${:cmd}} $c}]
	  #regexp {^(-?[0-9]+) +} $r0 _ mS0
	  regexp {^(-?[0-9]+) +} $r1 _ mS1
	  set ms [expr {$mS1*1.0/$c}]
	  puts stderr "[set :name]:\t[format %6.2f $ms] mms, ${:msg}"
	} else {
	  puts stderr "[set :name]: ${:msg} ok"
	}
      } else {
	puts stderr "[set :name]:\tincorrect result for '${:msg}'"
	puts stderr "expected:\n'${:expected}', got\n'$r'"
	puts stderr "\tin test file [info script]"
	if {[info exists :errorReport]} {eval [set :errorReport]}
	exit -1
      }
      if {[info exists :post]} {:call "post" ${:post}}
    }

  }
  ::namespace export Test
}

proc ? {cmd expected {msg ""}} {
  set namespace [uplevel {::namespace current}]
  if {[string match ::xotcl* $namespace]} {set namespace ::}
  #puts stderr "eval in namespace $namespace"
  if {$msg ne ""} {
    set t [Test new -cmd $cmd -msg $msg -namespace $namespace]
  } else {
    set t [Test new -cmd $cmd -namespace $namespace]
  }
  $t expected $expected 
  $t run
  nsf::__db_run_assertions
}


namespace import ::nx::test::*
