#!/usr/bin/env perl
use strict;
use warnings;
use Math::Trig;

my $CFG_TERM_SERIAL_R = 1000.0;
my $CFG_TERM_VALUE = 100000.0;
my $CFG_TERM_VALUE_TEMP = 25.0;
my $CFG_TERM_B_COEFF = 3950.0;

open(my $fh, '>', 'temp_table.h') or die $!;

print $fh "// Таблица для Arduino\nconst int tempTable[] PROGMEM = {\n";

for (my $i = 0; $i < 1024; $i++) {
    my $t;
    print("$i\n");
    if (($i >= 0)&&($i < 5)) { 
        $t = -1; 
    } else {
        my $res = 1023.0 / $i - 1.0;
        if($res != 0.0) {
          $res = $CFG_TERM_SERIAL_R / $res;
        
          my $st = log($res / $CFG_TERM_VALUE);
          $st /= $CFG_TERM_B_COEFF;
          $st += 1.0 / ($CFG_TERM_VALUE_TEMP + 273.15);
          $t = int((1.0 / $st - 273.15 + 0.5)*10.0);
        } else {
          $t = -1;   
        }
    }
    
    
    printf $fh "%4d", $t;
    print $fh ", " if $i < 1023;
    print $fh "\n" if ($i + 1) % 16 == 0;
}

print $fh "};\n";
close($fh);
print "Done!\n";