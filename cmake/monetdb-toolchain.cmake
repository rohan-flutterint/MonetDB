#[[
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0.  If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Copyright 1997 - July 2008 CWI, August 2008 - 2021 MonetDB B.V.
#]]

function(monetdb_default_toolchain)
  if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
      MT_addCompilerFlag("-fdiagnostics-color=always" "-fdiagnostics-color=always" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
    elseif ("${CMAKE_C_COMPILER_ID}" MATCHES "^(Clang|AppleClang)$")
      MT_addCompilerFlag("-fcolor-diagnostics" "-fcolor-diagnostics" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
    endif ()
  endif ()

  if(SANITIZER)
    if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
      MT_addCompilerFlag("-fsanitize=address" "-fsanitize=address" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("-fno-omit-frame-pointer" "-fno-omit-frame-pointer" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      add_definitions(-DNO_ATOMIC_INSTRUCTIONS)
    else()
      message(FATAL_ERROR "Sanitizer only supported with GCC")
    endif()
  endif()

  if(STRICT)
    if(${CMAKE_C_COMPILER_ID} MATCHES "^(GNU|Clang|AppleClang)$")
      MT_addCompilerFlag("-Werror" "-Werror" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("-Wall" "-Wall" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("-Wextra" "-Wextra" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("-W" "-W" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_checkCompilerFlag("-Werror-implicit-function-declaration")

      MT_checkCompilerFlag("-Wpointer-arith")
      MT_checkCompilerFlag("-Wundef")
      MT_checkCompilerFlag("-Wformat=2")
      MT_checkCompilerFlag("-Wformat-overflow=1")
      MT_checkCompilerFlag("-Wno-format-truncation")
      MT_checkCompilerFlag("-Wno-format-nonliteral")
      #MT_checkCompilerFlag("-Wformat-signedness") 	-- numpy messes this up
      MT_checkCompilerFlag("-Wno-cast-function-type")
      MT_checkCompilerFlag("-Winit-self")
      MT_checkCompilerFlag("-Winvalid-pch")
      MT_checkCompilerFlag("-Wmissing-declarations")
      MT_checkCompilerFlag("-Wmissing-format-attribute")
      MT_checkCompilerFlag("-Wmissing-prototypes")
      # need this for clang 9.1.0 on Darwin:
      MT_checkCompilerFlag("-Wno-missing-field-initializers")
      MT_checkCompilerFlag("-Wold-style-definition")
      MT_checkCompilerFlag("-Wpacked")
      MT_checkCompilerFlag("-Wunknown-pragmas")
      MT_checkCompilerFlag("-Wvariadic-macros")
      MT_checkCompilerFlag("-Wstack-protector")
      MT_checkCompilerFlag("-fstack-protector-all")
      MT_checkCompilerFlag("-Wpacked-bitfield-compat")
      MT_checkCompilerFlag("-Wsync-nand")
      MT_checkCompilerFlag("-Wjump-misses-init")
      MT_checkCompilerFlag("-Wmissing-include-dirs")
      MT_checkCompilerFlag("-Wlogical-op")
      MT_checkCompilerFlag("-Wduplicated-cond")
      MT_checkCompilerFlag("-Wduplicated-branches")
      MT_checkCompilerFlag("-Wrestrict")
      MT_checkCompilerFlag("-Wnested-externs")
      MT_checkCompilerFlag("-Wmissing-noreturn")
      MT_checkCompilerFlag("-Wuninitialized")

      # since we use values of type "int8_t" as subscript,
      # and int8_t may be defined as plain "char", we cannot
      # allow this warning (part of -Wall)
      MT_checkCompilerFlag("-Wno-char-subscripts")

      MT_checkCompilerFlag("-Wunreachable-code")

    elseif(${CMAKE_C_COMPILER_ID} STREQUAL "Intel")
      if(WIN32)
        set(COMPILER_OPTION "/")
        MT_addCompilerFlag("${COMPILER_OPTION}W3" "${COMPILER_OPTION}W3" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
        MT_addCompilerFlag("${COMPILER_OPTION}Qdiag-disable:11074" "${COMPILER_OPTION}Qdiag-disable:11074" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
        MT_addCompilerFlag("${COMPILER_OPTION}Qdiag-disable:11075" "${COMPILER_OPTION}Qdiag-disable:11075" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      else()
        set(COMPILER_OPTION "-")
        MT_addCompilerFlag("${COMPILER_OPTION}Wall" "${COMPILER_OPTION}Wall" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      endif()
      MT_addCompilerFlag("${COMPILER_OPTION}Wcheck" "${COMPILER_OPTION}Wcheck" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("${COMPILER_OPTION}Werror-all" "${COMPILER_OPTION}Werror-all" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
      MT_addCompilerFlag("${COMPILER_OPTION}${INTEL_OPTION_EXTRA}wd2259" "${COMPILER_OPTION}${INTEL_OPTION_EXTRA}wd2259" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
    elseif(MSVC)
      MT_addCompilerFlag("/WX" "/WX" "${CMAKE_C_FLAGS}" "all" CMAKE_C_FLAGS)
    endif()
  endif()

  if(NOT MSVC)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS};-Wno-unreachable-code")
    # Warning don't add '-' or '/' to the output variable!
    check_c_source_compiles("int main(int argc,char** argv){(void)argc;(void)argv;return 0;}"
      COMPILER_Wnounreachablecode)
    cmake_pop_check_state()
  endif()

  if(NOT ASSERT)
    MT_checkCompilerFlag("-DNDEBUG=1")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" PARENT_SCOPE)

endfunction()

function(monetdb_default_compiler_options)
  if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
      add_compile_options("-fdiagnostics-color=always")
    elseif ("${CMAKE_C_COMPILER_ID}" MATCHES "^(Clang|AppleClang)$")
      add_compile_options("-fcolor-diagnostics")
    endif ()
  endif ()

  if(SANITIZER)
    if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
      add_compile_options("-fsanitize=address")
      add_compile_options("-fno-omit-frame-pointer")
      add_compile_definitions(NO_ATOMIC_INSTRUCTIONS)
    else()
      message(FATAL_ERROR "Sanitizer only supported with GCC")
    endif()
  endif()

  if(STRICT)
    if(${CMAKE_C_COMPILER_ID} MATCHES "^(GNU|Clang|AppleClang)$")
      add_compile_options("-Werror")
      add_compile_options("-Wall")
      add_compile_options("-Wextra")
      add_compile_options("-W")

      add_option_if_available("-Werror-implicit-function-declaration")
      add_option_if_available("-Wpointer-arith")
      add_option_if_available("-Wundef")
      add_option_if_available("-Wformat=2")
      add_option_if_available("-Wformat-overflow=1")
      add_option_if_available("-Wno-format-truncation")
      add_option_if_available("-Wno-format-nonliteral")
      #add_option_if_available("-Wformat-signedness") 	-- numpy messes this up
      add_option_if_available("-Wno-cast-function-type")
      add_option_if_available("-Winit-self")
      add_option_if_available("-Winvalid-pch")
      add_option_if_available("-Wmissing-declarations")
      add_option_if_available("-Wmissing-format-attribute")
      add_option_if_available("-Wmissing-prototypes")
      # need this for clang 9.1.0 on Darwin:
      add_option_if_available("-Wno-missing-field-initializers")
      add_option_if_available("-Wold-style-definition")
      add_option_if_available("-Wpacked")
      add_option_if_available("-Wunknown-pragmas")
      add_option_if_available("-Wvariadic-macros")
      add_option_if_available("-Wstack-protector")
      add_option_if_available("-fstack-protector-all")
      add_option_if_available("-Wpacked-bitfield-compat")
      add_option_if_available("-Wsync-nand")
      add_option_if_available("-Wjump-misses-init")
      add_option_if_available("-Wmissing-include-dirs")
      add_option_if_available("-Wlogical-op")
      add_option_if_available("-Wduplicated-cond")
      add_option_if_available("-Wduplicated-branches")
      add_option_if_available("-Wrestrict")
      add_option_if_available("-Wnested-externs")
      add_option_if_available("-Wmissing-noreturn")
      add_option_if_available("-Wuninitialized")

      # since we use values of type "int8_t" as subscript,
      # and int8_t may be defined as plain "char", we cannot
      # allow this warning (part of -Wall)
      add_option_if_available("-Wno-char-subscripts")

      add_option_if_available("-Wunreachable-code")
    elseif(${CMAKE_C_COMPILER_ID} STREQUAL "Intel")
      if(WIN32)
        add_compile_options("/W3")
        add_compile_options("/Qdiag-disable:11074")
        add_compile_options("/Qdiag-disable:11075")
        add_compile_options("/Wcheck")
        add_compile_options("/Werror-all")
        add_compile_options("/${INTEL_OPTION_EXTRA}wd2259")
      else()
        add_compile_options("-Wall")
        add_compile_options("-Wcheck")
        add_compile_options("-Werror-all")
        add_compile_options("-${INTEL_OPTION_EXTRA}wd2259")
      endif()
    elseif(MSVC)
      add_compile_options("/WX")
    endif()
  endif()

  if(NOT MSVC)
    add_option_if_available("-Wno-unreachable-code")
  endif()

  if(NOT ASSERT)
     add_compile_definitions("NDEBUG=1")
  endif()
endfunction()
