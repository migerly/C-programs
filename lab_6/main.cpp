#include <iostream>
#include <coroutine>
#include <optional>

// -------------------- Генератор на базі сопрограм ------------------------

template<typename T>
class Generator {
public:
    struct promise_type {
        T current_value{};
        std::exception_ptr exception;

        Generator get_return_object() {
            using handle_type = std::coroutine_handle<promise_type>;
            return Generator{ handle_type::from_promise(*this) };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) noexcept {
            current_value = value;
            return {};
        }

        void unhandled_exception() {
            exception = std::current_exception();
        }

        void return_void() {}
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Generator(handle_type h) : coro(h) {}
    Generator(Generator&& other) noexcept : coro(other.coro) {
        other.coro = nullptr;
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    ~Generator() {
        if (coro) coro.destroy();
    }

    // Переходимо до наступного значення; повертає false, якщо корутина завершилась
    bool next() {
        if (!coro || coro.done())
            return false;

        coro.resume();

        if (coro.promise().exception)
            std::rethrow_exception(coro.promise().exception);

        return !coro.done();
    }

    // Поточне значення, "вироблене" co_yield
    T value() const {
        return coro.promise().current_value;
    }

private:
    handle_type coro;
};

// ---------------------- Глобальний стан гри ------------------------------

struct GameState {
    int low = 1;
    int high = 100;
};

GameState g_state;

// ----------------------- Сопрограма-«вгадувач» ---------------------------
// Використовує межі g_state.low / g_state.high, які змінює головна програма.

Generator<int> guessing_coroutine() {
    while (g_state.low <= g_state.high) {
        int guess = (g_state.low + g_state.high) / 2;
        co_yield guess;
        // Після co_yield корутина "засинає", а головна програма
        // дочекається реакції від користувача, оновить g_state.low/high
        // і знову викличе next(), відновивши виконання з наступної ітерації.
    }
    // Якщо межі "перехрестилися", просто завершуємо корутину.
    co_return;
}

// -------------------------- Демо-програма --------------------------------

int main() {
    setlocale(LC_ALL, "Ukr");
    std::cout << "Гра \"Вгадай число\" (1..100)\n";
    std::cout << "Загадайте число в голові.\n";
    std::cout << "Я ставитиму запитання, а ви відповідайте:\n";
    std::cout << "  -1  якщо моє число МЕНШЕ загаданого\n";
    std::cout << "   0  якщо я ВГАДАВ\n";
    std::cout << "   1  якщо моє число БІЛЬШЕ загаданого\n\n";

    // Початкові межі
    g_state.low = 1;
    g_state.high = 100;

    auto gen = guessing_coroutine();
    bool guessed = false;

    while (gen.next()) {
        int guess = gen.value();
        std::cout << "[Протокол] Моя спроба: " << guess << "\n";
        std::cout << "Ваша відповідь (-1 / 0 / 1): ";

        int reaction;
        while (true) {
            if (!(std::cin >> reaction)) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "Введіть, будь ласка, число -1, 0 або 1: ";
                continue;
            }
            if (reaction == -1 || reaction == 0 || reaction == 1) break;
            std::cout << "Введіть -1, 0 або 1: ";
        }

        if (reaction == 0) {
            std::cout << "Ура! Я вгадав ваше число: " << guess << "\n";
            guessed = true;
            break;
        }
        else if (reaction == -1) {
            // Моє число менше загаданого -> нижня межа зсувається вгору
            g_state.low = guess + 1;
        }
        else if (reaction == 1) {
            // Моє число більше загаданого -> верхня межа зсувається вниз
            g_state.high = guess - 1;
        }

        if (g_state.low > g_state.high) {
            break; // користувач "збрехав" або щось пішло не так
        }
    }

    if (!guessed) {
        std::cout << "Схоже, ми заплутались у відповідях :) "
            "Межі пошуку стали некоректними.\n";
    }

    std::cout << "Гру завершено.\n";
    return 0;
}
