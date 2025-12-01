#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <future>
#include <syncstream>

// Для зручності
using namespace std::chrono_literals;

// Універсальна "обчислювальна" функція
void compute(const std::string& name, int seconds)
{
    // моделюємо тривалість обчислення
    std::this_thread::sleep_for(std::chrono::seconds(seconds));

    // синхронізований вивід
    std::osyncstream out(std::cout);
    out << name << '\n';
}

// "Повільне" обчислення: 7 секунд
inline void slow(const std::string& name)
{
    compute(name, 7);
}

// "Швидке" обчислення: 1 секунда
inline void quick(const std::string& name)
{
    compute(name, 1);
}


void work()
{
    using clock = std::chrono::steady_clock;
    auto t_start = clock::now();

    // Додатковий потік: C2, потім D2
    auto fut = std::async(std::launch::async, [] {
        quick("C2");   // не має вхідних залежностей
        quick("D2");   // не має вхідних залежностей, але потрібна для F
        });

    // Головний потік: найдовший ланцюжок
    slow("A");   // повільне, 7 с
    slow("B");   // повільне, 7 с, B(A)
    quick("C1"); // швидке, C1(B)

    fut.get();   // гарантує виконання C2 та D2

    quick("D1"); // D1 залежить від C1 і C2
    quick("F");  // F залежить від D1 та D2

    auto t_end = clock::now();
    double seconds = std::chrono::duration<double>(t_end - t_start).count();

    {
        std::osyncstream out(std::cout);
        out << "Total time: " << seconds << " s\n";
        out << "Work is done!\n";
    }
}

// Демонстраційний main
int main()
{
    work();
    return 0;
}
