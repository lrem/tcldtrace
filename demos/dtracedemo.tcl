#!/usr/bin/tclsh
package require Tk
package require dtrace

set handle [::dtrace::open -foldpdesc 1]

image create photo logoObj -file "[file dirname [info script]]/logo.gif"
grid [label .logo -image logoObj] -columnspan 2

grid [labelframe .editor -text "Editor"]
grid [text .editor.code -width 80 -height 24] -columnspan 2
grid [button .editor.clear -text "Clear" -width 36] -row 1 -column 0
grid [button .editor.enable -text "Enable" -width 36] -row 1 -column 1
grid [button .editor.go -text "GO" -width 76] -row 2 -columnspan 2

proc clearEditor {} {
    .editor.code delete 1.0 end
}
.editor.clear configure -command clearEditor

proc enableProbe {} {
    global handle
    set source [.editor.code get 1.0 end]
    set program [::dtrace::compile $handle $source]
    ::dtrace::exec $program
    .editor.code delete 1.0 end
    .editor.code insert 1.0 "/* Program $program successfuly enabled */\n"
}
.editor.enable configure -command enableProbe

proc GO {} {
    global handle
    ::dtrace::go $handle probe_desc {descCallback {}}
    
    grid forget .editor
    
    grid [labelframe .aggregations -text "Aggregations"] -row 1 -column 0
    grid [text .aggregations.text -width 40 -height 100 -tabs {320 right}]

    grid [labelframe .output -text "Probes"] -row 1 -column 1
    grid [text .output.text -width 40 -height 100]

    dtraceLoop
}
.editor.go configure -command GO

proc descCallback {probe args} {
    .output.text insert 1.0 "$probe\n"
}

proc dtraceLoop {} {
    global handle
    ::dtrace::process $handle
    set aggdata [::dtrace::aggregations  $handle]
    .aggregations.text delete 1.0 end
    for {set i 0} {$i < [llength $aggdata]} {incr i 2} {
        .aggregations.text insert end\
        "[lindex $aggdata $i]\t[lindex $aggdata [expr {$i+1}]]\n"
    }
    after 300 dtraceLoop
}
