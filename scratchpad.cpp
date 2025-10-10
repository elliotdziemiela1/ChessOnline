#include <string>


enum Column {
    A = 1,
    B = 2,
    C = 3,
    D = 4,
    E = 5,
    F = 6,
    G = 7,
    H = 8
};

template <typename T>
void foo(){
    T tmp;
    return T;
}

int main () {
    enum Column t1 = B;

    std::string s1 = "B5";
    enum Column Column = (Column)(s1[0] - 'A' + 1);
    

    return 0;
}