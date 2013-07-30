# NXDoc index file, version 1.0
# This file was generated by the "::nx::doc::mkIndex" command
# and is optionally sourced by nxdoc to filter the command population
# to be documented.  Typically each line is a command that
# sets an element in the ::nxdoc::include array, where the
# element name is the name of a command and the value indicates whether
# the command is to be documented (1) or not (0).
#
# To regenerate this index file, run nxsh in the root dir of the
# source distribution and have the following two liner evaluated:
#
# package req nx::doc
# nx::doc::mkIndex -documentAll -indexfiles nsfAPI.nxdocindex -outdir generic/ generic/nsf.tcl
# 

# Source external (e.g., auto-generated) index files
source [file join [file dirname [info script]] nsfAPI.nxdocindex]
set ::nxdoc::include(::nsf::method::provide) 1
set ::nxdoc::include(::nsf::method::require) 1
set ::nxdoc::include(::nsf::strip_proc_name) 0
set ::nxdoc::include(::nsf::mixin) 1
set ::nxdoc::include(::nsf::object::unknown) 0
set ::nxdoc::include(::nsf::object::unknown::add) 0
set ::nxdoc::include(::nsf::object::unknown::get) 0
set ::nxdoc::include(::nsf::object::unknown::delete) 0
set ::nxdoc::include(::nsf::object::unknown::keys) 0
set ::nxdoc::include(::nsf::exithandler) 1
set ::nxdoc::include(::nsf::__exithandler) 0
set ::nxdoc::include(::nsf::log) 1
set ::nxdoc::include(::nsf::deprecated) 1
set ::nxdoc::include(::nsf::tmpdir) 1