#!/usr/bin/tclsh

package require dtrace

proc accept {sock addr port} {
    global handles scripts
    set handles($sock) [::dtrace::open -foldpdesc 1]
    set scripts($sock) ""
    fconfigure $sock -buffering line
    fileevent $sock readable [list receive $sock $addr $port]
    puts "Client connected from $addr:$port"
}

proc receive {sock addr port} {
    global handles scripts
    if {[eof $sock] || [catch {gets $sock line}]} {
        close $sock
        ::dtrace::close $handles($sock)
        unset handles($sock) scripts($sock)
        puts "Client $addr:$port disconnected"
    } else {
        if {[string equal $line "GO"]} {
            ::dtrace::exec [::dtrace::compile $handles($sock) $scripts($sock)]
            ::dtrace::go $handles($sock) probe_desc [list callback $sock]
            puts $sock "CPU\tid\tprobe"
            dtraceLoop $sock
            puts "Tracing for $addr:$port started"
        } else {
            set scripts($sock) "$scripts($sock)\n$line"
        }
    }
}

proc callback {probe cpu id sock} {
    puts $sock "$cpu\t$id\t$probe"
}

proc dtraceLoop {sock} {
    global handles
    catch {::dtrace::process $handles($sock)}
    after 300 dtraceLoop $sock
}

socket -server accept 1986
vwait forever

# vim: set ts=4 sw=4 et:
