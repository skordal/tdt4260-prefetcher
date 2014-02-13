// Modular integer datatype, à la Ada modular types

template<int modulo>
class modint
{
	public:
		modint(int init = 0) { value = init % modulo; }

		modint & operator=(int value) { this->value = value % modulo; return *this; }

		modint operator+(int add) { return modint<modulo>(value + add); }
		modint operator-(int sub) { return modint<modulo>((value + modulo - (sub % modulo))); }
		modint & operator++() { value = (value + modulo + 1) % modulo; return *this; }
		modint & operator--() { value = (value + modulo - 1) % modulo; return *this; }
		modint operator++(int) { int retval = value; value = (value + modulo + 1) % modulo; return modint<modulo>(retval); }
		modint operator--(int) { int retval = value; value = (value + modulo - 1) % modulo; return modint<modulo>(retval); }

		bool operator==(int value) const { return (value % modulo) == this->value; }
		bool operator==(const modint<modulo> & value) const { return this->value == value.value; }
		bool operator!=(int value) const { return !(*this == value); }
		bool operator!=(const modint<modulo> & value) const { return !(*this == value); }

		operator int() const { return value; }
	private:
		int value;
};

