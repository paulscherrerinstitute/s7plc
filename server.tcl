#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"
set upsize 80
set downsize 80

option add *font {courier 12}
option add *highlightThickness 1
option add *highlightThickness 1
option add *borderWidth 1

# may also load updata from file

set downdata [string repeat "\0" $downsize]
set updata [string repeat "\0" $upsize]

proc makeGUI {{parent {}}} {
    global upsize updata downsize downdata
    
    # create up buffer (where user can write to)
    set maxrow [expr ($upsize-1)/16+1]
    set f [frame $parent.input]
    for {set row 0} {$row < $maxrow} {incr row} {
        for {set col 0} {$col < 16} {incr col} {
            set e [entry $f.hexentry_${row}_$col \
                -width 2 -validate key \
                -validatecommand "validateHex %W $row $col %P %S %i"]
            bindtags $e [concat $e [winfo toplevel $e] Input InputHex]
            grid $e -row $row -column $col
        }
    }
    set i 0
    for {set row 0} {$row < $maxrow} {incr row} {
        set e [entry $f.stringentry_$row \
            -width 16 -validate key\
            -validatecommand "validateString %W $row %P %S %i"]
            bindtags $e [concat $e [winfo toplevel $e] Input InputString]
        grid $e -row $row -column $col
        for {set col 0} {$col < 16} {incr col} {
            if {$i < $upsize} {
                scan [string index $updata $i] "%c" v
                $f.hexentry_${row}_$col insert end [format "%02x" $v]
            } else {
                $f.hexentry_${row}_$col config -state disabled
            }
            incr i
        }
        $e icursor 0
    }
    
    # create down buffer (where values are displayed)
    set maxrow [expr ($downsize-1)/16+1]
    set baserow $row
    for {set row 0} {$row < $maxrow} {incr row} {
        for {set col 0} {$col < 16} {incr col} {
            set e [label $f.hexlabel_${row}_$col \
                -width 2 -relief raised]
            grid $e -row [expr $row+$baserow] -column $col
        }
        set e [label $f.stringlabel_$row \
            -width 16  -relief raised]
        grid $e -row [expr $row+$baserow] -column $col
    }
    trace variable downdata w "updataData $f"
    pack $f
    updataData $f
}

proc validateHex {w row col new ins index} {
    global updata
    if {![string is xdigit $ins] || [string length $new] > 2} {
        return false
    }
    set s [winfo parent $w].stringentry_$row
    $s config -validate none
    $s delete $col
    set v [expr 0x0$new]
    if {($v & 0x7f) < 0x20 || $v == 0x7f} {
        set c "."
    } else {
        set c [format "%c" $v]
    }
    $s insert $col $c
    $s config -validate key
    set i [expr $row*16+$col]
    set updata [string replace $updata $i $i [format "%c" $v]]
    if {$index == 1} {
        set w [tk_focusNext $w]
        focus $w
        $w icursor 0
    }
    return true
}

proc validateString {w row new ins index} {
    global upsize updata
    if {$row*16+[string length $new] > $upsize || $index>15} {
        return false
    }
    if {[string length $ins] != 1} {
        return false
    }
    scan $ins "%c" v
    if {($v & 0x7f) < 0x20 || $v == 0x7f} {
        return false
    }
    set s [format "%02x" $v]
    set h [winfo parent $w].hexentry_${row}_$index
    $h config -validate none
    $h delete 0 end
    $h insert 0 $s
    $h config -validate key
    set i [expr $row*16+$index]
    set updata [string replace $updata $i $i $ins]
    if {$index == 15} {
        set w [tk_focusNext $w]
        focus $w
        $w icursor 0
    }
    return true
}

proc updataData {w args} {
    global downsize downdata
    set maxrow [expr ($downsize-1)/16+1]
    set row 0
    set col 0
    set i 0
    for {set row 0} {$row < $maxrow} {incr row} {
        set s ""
        for {set col 0} {$col < 16} {incr col} {
            scan [string index $downdata $i] "%c" v
            $w.hexlabel_${row}_$col config -text [format "%02x" $v]
            if {($v & 0x7f) < 0x20 || $v == 0x7f} {
                set c "."
            } else {
                set c [format "%c" $v]
            }
            append s $c
            incr i
        }
        $w.stringlabel_${row} config -text $s
    }
}

bind Input <Return> {focus [tk_focusNext %W]}
bind Input <Key> {if [string length %A] {%W delete insert; %W insert insert %A}}
bind Input <Left> {if {[%W index insert]==0} {focus [tk_focusPrev %W]} {%W icursor [expr {[%W index insert] - 1}]}}
bind Input <Right> {if {[%W index insert]==[string length [%W get]]-1} {focus [tk_focusNext %W]; catch {[focus] icursor 0}} {%W icursor [expr {[%W index insert] + 1}]}}
bind Input <Tab> {focus [tk_focusNext %W]}
bind Input <BackSpace> {%W icursor [expr {[%W index insert] - 1}]}
bind Input <Delete> {;}
bind Input <1> {if {[%W cget -state] != "disabled"} {focus %W; %W icursor @%x}}

bind InputHex <FocusOut> {while {[string length [%W get]] < 2} {%W insert 0 "0"}}

bind InputString <Down> {focus [tk_focusNext %W]}
bind InputString <Up> {focus [tk_focusPrev %W]}

makeGUI

set serversocket [socket -server connect 2000]

proc connect {sock addr port} {
 
  puts "Connect to : $addr , $port"
  
  fconfigure $sock -translation binary -blocking 0 -buffering none
  fileevent $sock readable "receiveHandler $sock"
  writeLoop $sock
}

proc receiveHandler {sock} {
    global downdata
    set a [read $sock]
    set l [string length $a]
    set downdata [string replace $downdata 0 [expr $l-1] $a]
    if [eof $sock] {
        puts "connection closed by peer"
        close $sock
        return
    }
    puts "$l bytes data received"
#    for {set i 0} {$i < $l} {incr i} {
#        scan [string index $a $i] "%c" c
#        puts -nonewline [format "%02x " $c]
#    }
#    puts ""
}

proc writeLoop {sock} {
    global updata
    if [catch {puts -nonewline $sock $updata}] return
    set l [string length $updata]
   puts "$l bytes data sent"
#    for {set i 0} {$i < $l} {incr i} {
#        scan [string index $updata $i] "%c" c
#        puts -nonewline [format "%02x " $c]
#    }
#    puts ""
    after 1000 writeLoop $sock
}
    
