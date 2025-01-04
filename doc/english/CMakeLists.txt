add_custom_command(OUTPUT DagOnFS.ps
                   COMMAND groff -e -T ps -man ${CMAKE_CURRENT_SOURCE_DIR}/DagOnFS.man > DagOnFS.ps
                   DEPENDS DagOnFS.man
                   COMMENT "Creating postscript file")
add_custom_target(postscript ALL
                  DEPENDS DagOnFS.ps)

# Set GROFF_NO_SGR because the following txt rule running ul is incompatible with the 
# default.
add_custom_command(OUTPUT DagOnFS.1
                   COMMAND export GROFF_NO_SGR=1 && groff -e -T ascii -man ${CMAKE_CURRENT_SOURCE_DIR}/DagOnFS.man > DagOnFS.1
                   DEPENDS DagOnFS.man
                   COMMENT "Creating man page")
add_custom_target(man ALL
                  DEPENDS DagOnFS.1)

add_custom_command(OUTPUT DagOnFS.txt
                   COMMAND ul -t dumb DagOnFS.1 > DagOnFS.txt
                   DEPENDS DagOnFS.1
                   COMMENT "Creating text file")
add_custom_target(txt ALL
                  DEPENDS DagOnFS.txt)

add_custom_command(OUTPUT DagOnFS.pdf
                   COMMAND ps2pdf DagOnFS.ps > DagOnFS.pdf
                   DEPENDS DagOnFS.ps
                   COMMENT "Creating pdf file")
add_custom_target(pdf ALL
                  DEPENDS DagOnFS.pdf)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/DagOnFS.ps" DESTINATION "${DOCDIR}")
install(FILES DagOnFS.man DESTINATION "${MANDIR}/man1" RENAME DagOnFS.1)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/DagOnFS.txt" DESTINATION "${DOCDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/DagOnFS.pdf" DESTINATION "${DOCDIR}")
