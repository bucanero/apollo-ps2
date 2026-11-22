# Copies a .po file without what mini18n doesn't use (the header, comments and
# blank lines), for the translations embedded in the ELF.
# usage: cmake -DIN=lang_xx.po -DOUT=stripped/lang_xx.po -P strip_po.cmake

file(READ "${IN}" po)
set(po "\n${po}\n")
string(REPLACE "\r" "" po "${po}")
# the header: msgid "" + msgstr "" and its "..." lines
string(REGEX REPLACE "\nmsgid \"\"\nmsgstr \"\"(\n\"[^\n]*\")*" "" po "${po}")
# comment lines
string(REGEX REPLACE "\n#[^\n]*" "" po "${po}")
# blank lines
string(REGEX REPLACE "\n\n+" "\n" po "${po}")
string(REGEX REPLACE "^\n" "" po "${po}")
file(WRITE "${OUT}" "${po}")
