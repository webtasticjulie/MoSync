/* Copyright (C) 2010 Mobile Sorcery AB

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
*/

/**
* \file Style
* \brief Classes for managing styles
* \author Patrick Broman and Niklas Nummelin
*/

#ifndef _MAUI_STYLE_H_
#define _MAUI_STYLE_H_

#include <ma.h>
#include <MAUtil/Vector.h>

namespace MAUI {

class Property {
public:
	enum Type {
		COLOR,
		FONT,
		IMAGE,
		SKIN
	};

	Property(Type type);


	Type getType() const { return mType; }

	private:
		Type mType;

protected:
};

class Color : public Property {
public:
	Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : Property(sType), r(r), g(g), b(b), a(a) {
	}

	unsigned char r, g, b, a;
private:
	static Type sType;
};

class Style {
public:
	Style(int numProperties);

	template<typename T>
	T* get(int id) {
		Property* prop = mProperties[id];
		if(prop->getType()==T::sType) return (T*)prop;
		else return NULL;

	}

protected:
	MAUtil::Vector<Property*> mProperties;
};

} // namespace MAUI

#endif // _MAUI_STYLE_H_
