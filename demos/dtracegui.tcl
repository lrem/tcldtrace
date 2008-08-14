#!/opt/ActiveTcl-8.5/bin/tclsh

# Copyright header {{{
#
# Copyright (c) 2008, Remigiusz 'lRem' Modrzejewski
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Remigiusz Modrzejewski nor the
#       names of his contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Remigiusz Modrzejewski ''AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Remigiusz Modrzejewski BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# }}}
# Includes {{{

package require Tk
package require dtrace

# }}}
# Screen layout {{{

image create photo logoObj -file "[file dirname [info script]]/logo.gif"
grid [label .logo -image logoObj] -columnspan 2

grid [labelframe .editor -text "Editor"] -row 1 -column 0
grid [text .editor.code -width 60 -height 12] -columnspan 2
grid [button .editor.clear -text "Clear" -command clearEditor] \
    -row 1 -column 0 -sticky "ew"
grid [button .editor.enable -text "Enable" -command enableProbe] \
    -row 1 -column 1 -sticky "ew"
grid [button .editor.killall -text "Disalbe all probes" -command armageddon] \
    -row 2 -column 0 -sticky "ew"
grid [button .editor.reverse -text "Reverse aggregations" -command reverse] \
    -row 2 -column 1 -sticky "ew"

grid [labelframe .active -text "Active probes"] -row 1 -column 1 -sticky "nsew"

grid [labelframe .aggregations -text "Aggregations"] -row 2 -column 0
grid [text .aggregations.text -width 60 -height 100 -tabs {400 right} \
-state disabled]

grid [labelframe .output -text "Probes"] -row 2 -column 1
grid [text .output.text -width 60 -height 100 -state disabled]

# }}}
# Simple button actions {{{

proc clearEditor {} {
    .editor.code delete 1.0 end
}

proc reverse {} {
    global order
    set order [expr {!$order}]
}
set order 0

proc armageddon {} {
    foreach button [grid slaves .active] {$button invoke}
}

#}}}
# Probe enabling/disabling {{{

proc enableProbe {} {
    global topId
    incr topId
    set source [.editor.code get 1.0 end]
    set head [.editor.code get 1.0 1.end]
    set handle [dtrace open -foldpdesc 1]
    if {[catch {dtrace exec [dtrace compile $handle $source]} err]} {
    	dtrace close $handle
	error $err
    }
    grid [button .active.$topId -command [list disableProbe $topId $handle]\
    	-text "Disable probe $topId ($head)"] -sticky "ew"
    dtrace go $handle probe_desc {descCallback {}}
    dtraceLoop $handle $topId
}

proc disableProbe {id handle} {
    dtrace close $handle
    grid forget .active.$id
}

# }}}
# Output handing and dtraceLoop {{{

proc descCallback {probe args} {
    .output.text configure -state normal
    .output.text insert 1.0 "$probe\n"
    .output.text configure -state disabled
}

proc dtraceLoop {handle id} {
    if {[catch {dtrace process $handle}]} {
        .output.text configure -state normal
        .output.text insert 1.0 "Probe $id disabled.\n"
        .output.text configure -state disabled
        catch {dtrace close $handle}
        catch {grid forget .active.$id}
    } else {
        global order
        set aggdata [dtrace aggregations  $handle]
        if {$order} {set aggdata [lreverse $aggdata]}
        .aggregations.text configure -state normal
        .aggregations.text delete 1.0 end
        for {set i $order} {$i < [llength $aggdata]} {incr i 2} {
            if {$order} {set j [expr $i-1]} else {set j [expr $i+1]}
            .aggregations.text insert end\
                "[lindex $aggdata $i]\t[lindex $aggdata $j]\n"
        }
        .aggregations.text configure -state disabled
        after 300 [list dtraceLoop $handle $id]
    }
}
#}}}

# vim: set ts=4 sw=4 et foldmethod=marker:
