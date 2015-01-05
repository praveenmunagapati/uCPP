//                              -*- Mode: C++ -*- 
// 
// uC++ Version 6.1.0, Copyright (C) Richard C. Bilson 2006
// 
// uDefaultProcessors.cc -- 
// 
// Author           : Richard C. Bilson
// Created On       : Tue Aug  8 16:53:43 2006
// Last Modified By : Peter A. Buhr
// Last Modified On : Mon May 19 22:40:34 2008
// Update Count     : 3
//
// This  library is free  software; you  can redistribute  it and/or  modify it
// under the terms of the GNU Lesser General Public License as published by the
// Free Software  Foundation; either  version 2.1 of  the License, or  (at your
// option) any later version.
// 
// This library is distributed in the  hope that it will be useful, but WITHOUT
// ANY  WARRANTY;  without even  the  implied  warranty  of MERCHANTABILITY  or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
// for more details.
// 
// You should  have received a  copy of the  GNU Lesser General  Public License
// along  with this library.
// 


#include <uDefault.h>


// Must be a separate translation unit so that an application can redefine this routine and the loader does not link
// this routine from the uC++ standard library.


unsigned int uDefaultProcessors() {
    return __U_DEFAULT_PROCESSORS__;
} // uDefaultProcessors


// Local Variables: //
// compile-command: "make install" //
// End: //
