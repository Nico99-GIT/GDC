# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/workspaces/GDC/build/_deps/bindings-src"
  "/workspaces/GDC/build/_deps/bindings-build"
  "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix"
  "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/tmp"
  "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp"
  "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/src"
  "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
