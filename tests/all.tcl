#!/bin/sh
# \
exec tclsh "$0" ${1+"$@"}
#
# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2000 by Ajuba Solutions
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id$

package require Tcl 8.4
package require tcltest 2.2
namespace import ::tcltest::*
eval configure $argv -testdir [list [file dir [info script]]]
runAllTests
