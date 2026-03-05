# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/workspaces/GDC/build/_deps/nontype_functional-src"
  "/workspaces/GDC/build/_deps/nontype_functional-build"
  "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix"
  "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/tmp"
  "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/src/nontype_functional-populate-stamp"
  "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/src"
  "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/src/nontype_functional-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/src/nontype_functional-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/nontype_functional-subbuild/nontype_functional-populate-prefix/src/nontype_functional-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
