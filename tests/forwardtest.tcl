# -*- Tcl -*-
package require nx; namespace import ::nx::*
package require nx::test

###########################################
# trivial object delegation
###########################################
Test case delegation {
    Object create dog 
    Object create tail {
	:method wag args { return $args }
    }
    dog forward wag tail %proc
    
    ? {dog wag 100} 100
}


###########################################
# evaluating in scope 
###########################################
Test case inscope {
    Class create X {
	:attribute {x 1}
	:forward Incr -objscope incr
    }

    X create x1 -x 100
    x1 Incr x
    x1 Incr x
    x1 Incr x
    ? {x1 x} 103
}

###########################################
# adding 
###########################################
Test case adding {
    Object create obj {
	:forward addOne expr 1 +
    }

    ? {obj addOne 5} 6
}

###########################################
# more arguments
###########################################
Test case multiple-args {
    Object create target {
	:method foo args {return $args}
    }
    Object create obj {
	:forward foo target %proc %self a1 a2
    }

    ? {obj foo x1 x2} [list ::obj a1 a2 x1 x2]

    obj forward foo target %proc %self %%self %%p
    ? {obj foo x1 x2} [list ::obj %self %p x1 x2]
}

###########################################
# mixin example
###########################################
Test case mixin-via-forward {
    Object create mixin {
	:method unknown {m args} {return [concat [current] $m $args]}
    }
 
    Object create obj {
	:forward Mixin mixin %1 %self
    }

    ? {obj Mixin add M1} [list ::mixin add ::obj M1]
    ? {catch {obj Mixin}} 1
    
    obj forward Mixin mixin "%1 {Getter Setter}" %self
    ? {obj Mixin add M1} [list ::mixin add ::obj M1]
    ? {obj Mixin M1} [list ::mixin Setter ::obj M1]
    ? {obj Mixin} [list ::mixin Getter ::obj]
}


###########################################
# sketching extensibe info
###########################################
Test case info-via-forward {
    Object create Info {
	:method @mixin {o} {
	    $o info mixin
	}
	:method @class {o} { ;# without prefix, doing here a [Info class] wod be wrong
	    $o info class
	}
	:method @help {o} { ;# define a new subcommand for info
	    foreach c [:info methods] {lappend result [string range $c 1 end]}
	    return $result
	}
    }
    Object forward Info -methodprefix @ Info %1 %self

    Class create X {
	:create x1
    }
    ? {x1 Info class} ::X
    ? {x1 Info help} [list help mixin class]
}

###########################################
# variations of placement of options
###########################################
Test case incr {
    Object create obj {
	set :x 1
	:forward i1 -objscope incr x 
    }

    ? {obj i1} 2
}

###########################################
# introspeciton options
###########################################
Test case introspection {
    Class create C {
	:forward Info -methodprefix @ Info %1 %self 
    }

    ? {C info forward} Info
    C forward XXXo x
    ? {lsort [C info forward]} [list Info XXXo]

    ? {C info forward X*} [list XXXo]
    ? {lsort [C info forward *o]} [list Info XXXo]

    # delete the forwarder
    C method XXXo {} {}
    ? {C info forward} [list Info]

    # get the definition of a instforwarder
    ? {C info method definition Info} [list ::C forward Info -methodprefix @ Info %1 %self]

    # check introspection for objects
    Object create obj {
	:forward i1 -objscope incr x 
	:forward Mixin mixin %1 %self
	:forward foo target %proc %self %%self %%p
	:forward addOne expr 1 +
    }

    ? {lsort [obj info forward]} "Mixin addOne foo i1"
    ? {obj info forward -definition Mixin} "mixin %1 %self"
    ? {obj info forward -definition addOne} "expr 1 +"
    ? {obj info forward -definition foo} "target %proc %self %%self %%p"
    ? {obj info forward -definition i1} "-objscope ::incr x"
}

###########################################
# test serializer
###########################################
package require nx::serializer
Test case serializer {
    Object create obj {
        :method test {} {puts "i am [current proc]"}
    }
    set ::a [Serializer deepSerialize obj]
    #puts <<$::a>>
    eval $::a
    ? {set ::a} [Serializer deepSerialize obj]
}

###########################################
# test optional target cmd
###########################################
Test case optional-target {
    Object create obj {
	set :x 2
	:forward append -objscope
    }
    ? {obj append x y z} 2yz

    Object create n; Object create n::x {:method current {} {current}}
    Object create o
    o forward ::n::x
    ? {o x current} ::n::x
}

###########################################
# arg including instvar
###########################################
Test case percent-cmd {
    Object create obj {
	set :x 10
	:forward x* expr {%:eval {set :x}} *
    }
    ? {obj x* 10} "100"
}

###########################################
# positional arguments
###########################################
Test case positioning-args {
    Object create obj
    obj forward @end-13 list {%@end 13}
    ? {obj @end-13 1 2 3 } [list 1 2 3 13]

    obj forward @-1-13 list {%@-1 13}
    ? {obj @-1-13 1 2 3 } [list 1 2 13 3]

    obj forward @1-13 list {%@1 13}
    ? {obj @1-13 1 2 3 } [list 13 1 2 3]
    ? {obj @1-13} [list 13]

    obj forward @2-13 list {%@2 13}
    ? {obj @2-13 1 2 3 } [list 1 13 2 3]

    obj forward @list 10 {%@0 list} {%@end 99}
    ? {obj @list} [list 10 99]
    ? {obj @list a b c} [list 10 a b c 99]

    obj forward @list {%@end 99} {%@0 list} 10
    ? {obj @list} [list 10 99]
    ? {obj @list a b c} [list 10 a b c 99]
    
    obj forward @list {%@2 2} {%@1 1} {%@0 list}
    ? {obj @list} [list 1 2]
    ? {obj @list a b c} [list 1 2 a b c]
    
    obj forward @list x y z {%@0 list} {%@1 1} {%@2 2} 
    ? {obj @list} [list 1 2 x y z]
    ? {obj @list a b c} [list 1 2 x y z a b c]
    
    obj forward @list x y z {%@2 2} {%@1 1} {%@0 list}
    ? {obj @list} [list x 1 y 2 z]
    ? {obj @list a b c} [list x 1 y 2 z a b c]
    
    # adding some test cases which cover the interactions
    # between %@POS and %1 substitutions
    #
    
    obj forward @end-13 list {%@end 13} %1 %self
    ? {obj @end-13 1 2 3 } [list 1 ::obj 2 3 13]
    
    obj forward @end-13 list %1 {%@end 13} %self
    ? {obj @end-13 1 2 3 } [list 1 ::obj 2 3 13]
    
    obj forward @end-13 list {%@end 13} %1 %1 %1 %self
    ? {obj @end-13 1 2 3 } [list 1 1 1 ::obj 2 3 13]
    
    obj forward @end-13 list {%@-1 13} %1 %self
    ? {obj @end-13 1 2 3 } [list 1 ::obj 2 13 3]
    
    obj forward @end-13 list {%@1 13} %1 %self
    ? {obj @end-13 1 2 3 } [list 13 1 ::obj 2 3]
}

###############################################
# substitution depending on number of arguments
###############################################
Test case num-args {
    Object create obj {
	:forward f %self [list %argclindex [list a b c]]
	:method a args {return [list [current proc] $args]}
	:method b args {return [list [current proc] $args]}
	:method c args {return [list [current proc] $args]}
    }
    ? {obj f} [list a {}]
    ? {obj f 1 } [list b 1]
    ? {obj f 1 2} [list c {1 2}]
    ? {catch {obj f 1 2 3}} 1
}

###############################################
# option earlybinding
###############################################
Test case earlybinding {
    Object create obj {
	:forward s -earlybinding ::set ::X
    }
    ? {obj s 100} 100
    ? {obj s} 100

    Object method f args { next }          

    Class create NS	
    Class create NS::Main {
	:object method m1 {} { :m2 }
	:object method m2 {} {
	    ? {namespace eval :: {Object create toplevelObj1}} ::toplevelObj1
  
	    ? [list set _ [namespace current]] ::NS
	    ? [list set _ [NS create m1]] ::NS::m1
	    NS filter f
	    ? [list set _ [NS create m2]] ::NS::m2
	    NS filter ""

	    namespace eval ::test {
		? [list set _ [NS create m3]] ::test::m3
		NS filter f
		? [list set _ [NS create m4]] ::test::m4
		NS filter ""
	    }
	    
	    namespace eval test {
		? [list set _ [NS create m5]] ::NS::test::m5
		NS filter f
		? [list set _ [NS create m6]] ::NS::test::m6
		NS filter ""
	    }   
	}

	:method i1 {} { :i2 }
	:method i2 {} {
	    ? {namespace eval :: {Object create toplevelObj2}} ::toplevelObj2
  
	    ? [list set _ [namespace current]] ::NS
	    ? [list set _ [NS create i1]] ::NS::i1
	    NS filter f
	    ? [list set _ [NS create i2]] ::NS::i2
	    NS filter ""

	    namespace eval ::test {
		? [list set _ [NS create i3]] ::test::i3
		NS filter f
		? [list set _ [NS create i4]] ::test::i4
		NS filter ""
	    }
	    
	    namespace eval test {
		? [list set _ [NS create i5]] ::NS::test::i5
		NS filter f
		? [list set _ [NS create i6]] ::NS::test::i6
		NS filter ""
	    }
	    
	}
    }
	
    #puts ====
    NS::Main m1
    NS::Main create m
    m i1

    #puts ====
    ? [list set _ [NS create n1]] ::n1
    NS filter f
    ? [list set _ [NS create n2]] ::n2
    NS filter ""
    
    #puts ====
    namespace eval test {
	? [list set _ [NS create n1]] ::test::n1
	? [list set _ [NS create n3]] ::test::n3
	NS filter f
	? [list set _ [NS create n4]] ::test::n4
	NS filter ""
    }
}
###########################################
# forward to expr + callstack
###########################################
Test case callstack {
    Object forward expr -objscope

    Class create C {
	:method xx {} {current}
	:object method t {o expr} {
	    return [$o expr $expr]
	}
    }
    C create c1

    ? {c1 expr {[current]}} 		::c1
    ? {c1 expr {[current] == "::c1"}} 	1
    ? {c1 expr {[:xx]}} 		::c1
    ? {c1 expr {[:info class]}} 	::C
    ? {c1 expr {[:info is type C]}} 	1
    ? {c1 expr {[:info is type ::C]}} 	1

    ? {C t ::c1 {[current]}} 		::c1
    ? {C t ::c1 {[current] == "::c1"}} 1
    ? {C t ::c1 {[:xx]}} 		::c1
    ? {C t ::c1 {[:info class]}} ::C
    ? {C t ::c1 {[:info is type C]}} 	1
    ? {C t ::c1 {[:info is type ::C]}} 	1

    Object method expr {} {}

}