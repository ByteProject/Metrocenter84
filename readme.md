
# RetroInform | Metrocenter '84

RetroInform is an Inform 6 variant optimized for classic 8-bit and 16-bit computer systems. It consists of a forked version of the Inform 6.15 compiler and Metrocenter '84, a library that allows you to create very small Z-code files while retaining the typical Inform experience. Z-machine v3 is the standard target but the full range of v3 to v8 is supported. Metrocenter '84 is squeezed down to 20k for a game containing one object and one room. The library's name derives from [this song by Sunset Neon](https://www.youtube.com/watch?v=wKK4HIkepuY). See it as a journey back to the glory days of cheeky pop culture, where everything was still measured in kilobytes. We love the 80s and the text adventure heritage the decade brought to this world. If you ever felt the desire to create an Infocom style interactive fiction game for one of the home computers of your childhood, you came to the right place. Some stories are not told yet and we can't wait to see yours.

<a href="Hibernated 2 running in our own C64 interpreter"><img src="https://p196.p4.n0.cdn.getcloudapp.com/items/Apujqlg7/hibernated2_screen_metro84.png" align="left" height="540" width="763" ></a>

## Origins

Metrocenter '84 started as an in-house project at [Puddle Software](http://puddlesoft.net/), with the purpose of writing a sophisticated sequel to our award-winning adventure [Hibernated](https://8bitgames.itch.io/hibernated1). We used the wonderful mInform library by Dave Bernazzani as a base and then ended up adding new features, backporting routines and fixing bugs. The changelog in this repository helps you understand the progress. In our aim to provide a single place with all the tools and knowledge to create interactive fiction for retro systems, we also added Andy Kosela's amazing i6 utility as well as Sean Barrett's varying strings compiler code.

## Status

Metrocenter '84 library: `v1.0` | Inform compiler `v6.15R`.

The compiler is pretty much where we want it but we are actively working on the library, so feel free to come back every now an then and check if we pushed a new version. The project is considered stable and usable.

The Commodore 8-bit interpreter seen on the picture, which works on C64, C128, VIC-20, Plus/4 and PET, is not released yet. We plan to add it at a later date.

## Features

* the compiler supports varying strings and is built upon the last version to properly target Z3
* metro_scenery lib function for flexible, memory efficient, non-object consuming scenery
* clothing is disabled by default, you can either save 500 bytes or enable it
* several usefuel lib features have been implemented or backported from later Inform versions
* by standard the library uses a supplied set of abbreviations but you may add your own as well
* it's possible to completely disable Inform's built-in inference
* when compiling Z3, an Atari 8-bit disk image is created too, enabling you to instantly test your code on a proper home computer

## Documentation and examples

Please head to the [wiki](https://github.com/ByteProject/RetroInform/wiki) for an extensive documentation. In addition, we provided a reasonable set of tutorial files, literature and references in the `tutor` folder. A template (shell) as a starting point for your own adventure, a sample game (the famous Cloak of Darkness) and various files showcasing features unique to RetroInform and Metrocenter '84 can be found in `demos`.

## Building and installing

You need a Unix-like operating system to take full advantage of RetroInform. We recommend Linux but macOS or BSD should be fine as well. Feel free to have a look at the Makefile. In most cases you only need to: 

  `$ make`
  
  `$ make install`

## Usage

  `$ i6 game.inf`

This will create game.z3 and game.atr. Yes, it's that simple.

## Credits

Thanks to Graham Nelson, words can hardly express his gift to the interactive fiction community. Furthermore, the following amazing human beings have contributed directly or indirectly to this project: Dave Bernazzani, Fredrik Ramsberg, Sean Barrett, Hugo Labrande, Andy Kosela, Roger Firth, Sonja Kesserich, Andrew Plotkin, Lea Albaugh, Gareth Rees, Will Crowther, Don Woods, Donald Ekman, David M. Baggett, Scott Adams, Joe W. Aultman, David Cornelson.

## Resources

http://www.inform-fiction.org

http://www.brasslantern.org

http://www.ifarchive.org

https://ifdb.tads.org/

http://en.wikipedia.org/wiki/Interactive_fiction

## License

Just like the original Inform 6, the contents of this repository are published under the Artistic License 2.0. See file `license` for details.
