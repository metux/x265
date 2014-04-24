set(ASM_DIALECT "_YASM")
set(CMAKE_ASM${ASM_DIALECT}_SOURCE_FILE_EXTENSIONS asm)

if(X64)
    list(APPEND ASM_FLAGS -DARCH_X86_64=1 -DPIC)
    if(APPLE)
        set(ARGS -f macho64 -m amd64 -DPREFIX)
    elseif(UNIX AND NOT CYGWIN)
        set(ARGS -f elf64 -m amd64)
    else()
        set(ARGS -f win64 -m amd64)
    endif()
else()
    list(APPEND ASM_FLAGS -DARCH_X86_64=0)
    if(APPLE)
        set(ARGS -f macho)
    elseif(UNIX AND NOT CYGWIN)
        set(ARGS -f elf32)
    else()
        set(ARGS -f win32 -DPREFIX)
    endif()
endif()

# we cannot assume 16-byte stack alignment on x86_32 even with GCC
if(GCC AND X64)
    list(APPEND ASM_FLAGS -DHAVE_ALIGNED_STACK=1)
else()
    list(APPEND ASM_FLAGS -DHAVE_ALIGNED_STACK=0)
endif()

if(HIGH_BIT_DEPTH)
    list(APPEND ASM_FLAGS -DHIGH_BIT_DEPTH=1 -DBIT_DEPTH=10)
else()
    list(APPEND ASM_FLAGS -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8)
endif()
set(YASM_FLAGS ${ARGS} ${ASM_FLAGS} PARENT_SCOPE)
string(REPLACE ";" " " CMAKE_ASM_YASM_COMPILER_ARG1 "${ARGS}")

# This section exists to override the one in CMakeASMInformation.cmake
# (the default Information file). This removes the <FLAGS>
# thing so that your C compiler flags that have been set via
# set_target_properties don't get passed to yasm and confuse it.
if(NOT CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT)
    string(REPLACE ";" " " STR_ASM_FLAGS "${ASM_FLAGS}")
    set(CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT "<CMAKE_ASM${ASM_DIALECT}_COMPILER> ${STR_ASM_FLAGS} -o <OBJECT> <SOURCE>")
endif()

include(CMakeASMInformation)
set(ASM_DIALECT)
