#pragma once
#include <vector>
#include <type_traits>

namespace nodecxx {

template<class... E>
class EmittingEvents;

template<class H, class... R>
class EmittingEvents<H, R...> : public EmittingEvents<R...> {
protected:
    using type = H;
    using function_type = typename H::function_type;
private:
    std::vector<function_type> callbacks;
public:
    template<class T, class F>
    typename std::enable_if<std::is_same<T, H>::value, void>::type on(T, F&& callback) {
        callbacks.emplace_back(std::forward<F>(callback));
    }

    template<class T, class F>
    typename std::enable_if<!std::is_same<T, H>::value, void>::type on(T t, F&& callback) {
        EmittingEvents<R...>::on(t, std::forward<F>(callback));
    }
protected:
    template<class T, class... Args>
    typename std::enable_if<std::is_same<T, H>::value, bool>::type fireEvent(T, Args&&... args) {
        for (auto& callback : callbacks) {
            callback(std::forward<Args>(args)...);
        }
        return callbacks.size() > 0;
    }
    template<class T, class... Args>
    typename std::enable_if<!std::is_same<T, H>::value, bool>::type fireEvent(T t, Args&&... args) {
        return EmittingEvents<R...>::fireEvent(t, std::forward<Args>(args)...);
    }

    template<class T>
    typename std::enable_if<std::is_same<T, H>::value, void>::type clearListeners(T) {
        callbacks.clear();
    }
    template<class T>
    typename std::enable_if<!std::is_same<T, H>::value, void>::type clearListeners(T t) {
        EmittingEvents<R...>::clearListeners(t);
    }
}; 

template<>
class EmittingEvents<> {
};

}
