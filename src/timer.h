#define Colour24 uint32_t //24 bit colour


//union: three ways to reference the same four bytes of memory
//in memory: BBBBBBBB GGGGGGGG RRRRRRRR xxxxxxxx
union Colour {
    Colour24 raw24;
    byte array[3]; //0-2 are data, 3 is unused
    struct { //note backwards
        byte blue;
        byte green;
        byte red;
    };
    Colour(byte r, byte g, byte b){
        red = r;
        green = g;
        blue = b;
    }
    Colour(){}
};


