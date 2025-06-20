#ifndef ECS_GENERIC_SIGNATURE_INCLUDED
#define ECS_GENERIC_SIGNATURE_INCLUDED

#include "utils/ints.hpp"
#include "My/Bitset.hpp"

namespace ECS {

using ComponentID = Sint16;

#define ECS_MAX_COMPONENT 64
constexpr ComponentID MaxComponentID = ECS_MAX_COMPONENT;

template<class C>
constexpr ComponentID getID() {
    return C::ID;
}

template<class ...Cs>
constexpr static My::Bitset<MaxComponentID> getSignature() {
    constexpr ComponentID ids[] = {getID<Cs>() ...};
    // sum component signatures
    auto result = My::Bitset<MaxComponentID>(0);
    for (size_t i = 0; i < sizeof...(Cs); i++) {
        if (ids[i] < result.size())
            result.set(ids[i]);
    }
    return result;
}

struct Signature : My::Bitset<MaxComponentID> {
    private: using Base = My::Bitset<MaxComponentID>; public:

    constexpr Signature() {}

    constexpr Signature(typename Base::IntegerType startValue) : Base(startValue) {}

    constexpr Signature(const My::Bitset<MaxComponentID>& base) : Base(base) {}

    template<class C>
    constexpr bool getComponent() const {
        return this->operator[](C::ID);
    }

    template<class C>
    constexpr void setComponent(bool val) {
        My::Bitset<MaxComponentID>::set(C::ID, val);
    }

    template<class... Cs>
    constexpr bool hasComponents() const {
        return hasAll(getSignature<Cs...>());
    }
};

struct SignatureHash {
    size_t operator()(Signature self) const {
        //TODO: OMPTIMIZE improve hash
        // intsPerHash will always be 1 or greater as IntegerT cannot be larger than size_t
        constexpr size_t intsPerHash = sizeof(size_t) / Signature::IntegerSize;
        size_t hash = 0;
        for (int i = 0; i < self.nInts; i++) {
            for (int j = 0; j < intsPerHash; j++) {
                hash ^= (size_t)self.bits[i] << j * Signature::IntegerSize * CHAR_BIT;
            }
        }
        return hash;
    }
};

}

#endif