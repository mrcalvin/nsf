#
# Load Next Scripting Framework, XOTcl and some related packages for
# AOLserver 4.* and naviserver.  
#
#  - aolserver: rewrite essentially _ns_savenamespaces to include the
#    serialized objects
#
#  - naviserver: just needed for the package require
#    the serialization logic resides in ns/tcl/nstrace.tcl
#
# We expect to find the package in standard Tcl package search path
# (the auto_path var) The simplest location is to put them under the
# "lib" directory within the AOLserver tree.
#

package require XOTcl 2.0; namespace import -force ::xotcl::*
package require nx::serializer
ns_log notice "XOTcl version $::xotcl::version$::xotcl::patchlevel loaded"

if {[ns_info name] ne "NaviServer"} {
  #
  # We are loading into aolserver.
  #
  # Overload procedure defined in bin/init.tcl.
  # It is now XOTcl-savvy in how it treats some 
  # special namespaces.
  #
  
  proc _ns_savenamespaces {} {
    set script [_ns_getpackages]
    set import ""
    set nslist ""
    _ns_getnamespaces namespaces
    foreach n $namespaces {
      if {$n ne "::nsf" && $n ne "::xotcl" && $n ne "::nx" 
	  && ![string match "::nsf::*" $n]
	  && ![::nsf::object::exists $n]} {
	  lappend nslist $n
      }
    }

    foreach n $nslist {
        foreach {ns_script ns_import} [_ns_getscript $n] {
            append script [list namespace eval $n $ns_script] \n
            if {$ns_import ne ""} {
                append import [list namespace eval $n $ns_import] \n
            }
        }
    }
    if {[catch {::Serializer all} objects]} {
        ns_log notice "XOTcl extension not loaded; will not copy objects\
        (error: $objects; $::errorInfo)."
        set objects ""
    }

    ns_ictl save [append script \n \
	$objects \n $import]
    # just for debugging purposes
    if {0} {
      set f [open [::xotcl::tmpdir]/__aolserver-blueprint-[ns_info server].tcl w]
      puts $f $script
      close $f
    }
  }

  #
  # Source XOTcl files from shared/private library
  # the way AOLserver does for plain Tcl files.
  #
  
  proc _my_sourcefiles {shared private} {
    set files ""
    foreach file [lsort [glob -nocomplain -directory $shared *.xotcl]] {
        if {[file exists [file join $private [file tail $file]]] == 0} {
            lappend files $file
        }
    }
    foreach file [lsort [glob -nocomplain -directory $private *.xotcl]] {
        lappend files $file
    }
    foreach file $files {
        _ns_sourcefile $file
    }
  }

  ns_eval {
    _my_sourcefiles [ns_library shared] [ns_library private]
  }
}


