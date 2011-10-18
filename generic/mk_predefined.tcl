#!/bin/env tclsh
#
# A small script file that creates a static array from a tcl-
# script for inclusion in c programs      -gn
#
set f [open [lindex $argv 0]]
set content [read $f]
close $f

regsub -all {\\} $content && content
regsub -all {"} $content {\"} content             ;#"
regsub -all "\[ \]+\n" $content \n content        ;# remove trailing space
regsub -all "\n\[ \t\]+" $content \n content      ;# remove leading space
while {[regsub -all "\n#\[^\n\]*\n" $content \n content]>0} {
    ;# remove comment lines
}
regsub -all "\n#\[^\n\]*\n" $content \n content   ;# remove comment lines
regsub -all "\[\n\]+" $content \n content         ;# remove empty lines
regsub -all "\n}" $content "}" content            ;# newlines btwn braces
regsub -all "\n" $content "\\n\"\n\"" content

puts "static char cmd\[\] = "
puts "\"$content\";"
puts ""

