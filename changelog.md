# Changelog

## Metrocenter '84 Library

### 1.2

* constant ENABLE_NUMBERPARSING to activate numeral recognition
* number parsing logic ported from Inform 6.2 lib
* backported NumberWord() and TryNumber() routines
* added special_number globals
* number token implementation for grammar definitions

### 1.1

* updated codebase to use the standard Inform 6 compiler (6.34 or newer)
* inheritance of RT_Err disabled by default
* constant ENABLE_INFORM_RT_ERR to force Inform 6 runtime error inheritance

### 1.0

* constant ENABLE_WEAR activates clothing and wearable objects
* inference can be deactivated with DISABLE_INFERENCE constant
* lib function WordInPropery() has been backported
* the player / selfObject has now add_to_scope support
* add_to_scope empty stack bugfix backported
* bugfix in PrintCommand (Parser) affecting v3 dict. entries > 6 chars
* compass objects can be disabled using WITHOUT_DIRECTIONS constant
* hardcoded wall objects removed from compass implementation
* examining directions is recognized and triggers general response
* metro_scenery lib function for flexible, memory efficient, non-object consuming scenery
* custom abbreviations definition supported with CUSTOM_ABBREVIATIONS constant

### 0.9 beta

* forked from mInform 1.1
