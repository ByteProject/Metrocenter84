#!/usr/bin/env ruby
# --------------------------------------------------------------
# Metrocenter'84 Update 1.0
# Copyright (C) 2020 by Stefan Vogt.
# See file LICENSE for details.
# --------------------------------------------------------------

require 'open-uri'
require 'fileutils'

$is_windows = (ENV['OS'] == 'Windows_NT')

if $is_windows then
	# path on Windows
    $METROPATH = "C:\\YourProject\\MetroLib"
else
	# path on Linux
    $METROPATH = "/usr/share/retroinform/metro84"
end

$METRO_PARSER = File.join($METROPATH, 'parser.h')
$METRO_GRAMMAR = File.join($METROPATH, 'grammar.h')
$METRO_VERBLIB = File.join($METROPATH, 'verblib.h')
$EXT_SCENERY = File.join($METROPATH, 'scenery.h')
$EXT_FLAGS = File.join($METROPATH, 'flags.h')

puts "Metrocenter'84 Update 1.0"
puts "--------------------------------------"

puts "fetching parser.h"
File.open($METRO_PARSER, "w") do |file|
    file.write OpenURI.open_uri('https://raw.githubusercontent.com//ByteProject/Metrocenter84/master/metro84/parser.h').read
end

puts "fetching grammar.h"
File.open($METRO_GRAMMAR, "w") do |file|
    file.write OpenURI.open_uri('https://raw.githubusercontent.com//ByteProject/Metrocenter84/master/metro84/grammar.h').read
end

puts "fetching verblib.h"
File.open($METRO_VERBLIB, "w") do |file|
    file.write OpenURI.open_uri('https://raw.githubusercontent.com//ByteProject/Metrocenter84/master/metro84/verblib.h').read
end

puts "fetching scenery.h"
File.open($EXT_SCENERY, "w") do |file|
  file.write OpenURI.open_uri('https://raw.githubusercontent.com//ByteProject/Metrocenter84/master/metro84/scenery.h').read
end

puts "fetching flags.h"
File.open($EXT_FLAGS, "w") do |file|
    file.write OpenURI.open_uri('https://raw.githubusercontent.com//ByteProject/Metrocenter84/master/metro84/flags.h').read
end

puts "--------------------------------------"
puts "All files were updated."
