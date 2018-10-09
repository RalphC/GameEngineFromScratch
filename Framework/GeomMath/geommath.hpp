
namespace My {

    template<typename T, int ... Indexes>
    class swizzle {
        float v[sizeof...(Indexes)];

        public:
        T& operator=(const T& rhs) {
            int indexes[] = { Indexes... };
            for (int i = 0; i < sizeof...(Indexes); i++) {
                v[indexes[i]] = rhs[i];
            }
            return *(T*)this;
        }

        operator T() const {
            return T(v[Indexes]...)
        }
    };


    template<typename T>
    struct Vector2Type {
        union {
            T data[2];
            struct { T x, y; };
            struct { T r, g; };
            struct { T u, v; };
            swizzle<Vector2Type<T>, 0, 1> xy;
            swizzle<Vector2Type<T>, 1, 0> yx;
        }

        Vector2Type() {};
        Vector2Type(float _v) : x(_v), y(_v) {};
        Vector2Type(float _v, float _u) : x(_v), y(_u) {};
    };
}