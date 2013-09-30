TclDTrace
=========

DTrace is a system profiling suite created by Sun Microsystems. 
It allows the user to profile a program without modifying its text, 
or even without the need to distract it if it is running. 
It consists of a set of library hooks called 'providers', 
a set of tools, a C API (libdtrace) 
and its own specially crafted scripting language called D. 
A Tcl binding to libdtrace will allow users to run D scripts and process their results directly from Tcl. 
Together with DTrace providers already present in Tcl, 
this will form an efficient and convenient tool for performance analysis 
and has the potential to increase the productivity of Tcl developers.

This project can open a whole new world of exciting possibilities. 
I want to write some Tcl scripts to demonstrate some of them. 
The first thing that comes to mind after Tcl is Tk. 
Therefore most effort will be put into a Tk based visualisation. 
Some other initial ideas include statistical analysis of aggregated data 
and using raw data that 'dtrace' tool does not provide. 
The final shape of these scripts depends on the actual design of the API 
and probably ideas provided by the community in the meantime.

Read more:

- My own [libdtrace documentation](http://dev.lrem.net/tcldtrace/wiki/LibDtrace)
- [CommandsList](http://dev.lrem.net/tcldtrace/wiki/CommandsList) aka the API specification
- The basic [SampleProgram](http://dev.lrem.net/tcldtrace/wiki/SampleProgram)
- [IncludedDemos](http://dev.lrem.net/tcldtrace/wiki/IncludedDemos) - descriptions and screenshots 
