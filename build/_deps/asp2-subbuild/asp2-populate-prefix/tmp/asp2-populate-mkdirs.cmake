# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/workspaces/GDC/build/_deps/asp2-src"
  "/workspaces/GDC/build/_deps/asp2-build"
  "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix"
  "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/tmp"
  "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/src/asp2-populate-stamp"
  "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/src"
  "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/src/asp2-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/src/asp2-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/GDC/build/_deps/asp2-subbuild/asp2-populate-prefix/src/asp2-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
