#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;

// ===============================
// Структура заказа маркетплейса
// ===============================
struct Order {
    int id;
    int userId;
    int productId;
    int categoryId;
    double price;
    int quantity;
    double discountPercent;
    double deliveryCost;
    int statusId;
};

// ===============================
// Статистика обработки заказов
// ===============================
struct Statistics {
    double totalRevenue = 0.0;
    double marketplaceCommission = 0.0;
    double totalDeliveryCost = 0.0;
    double totalDiscount = 0.0;

    long long totalOrders = 0;
    long long totalItems = 0;
    long long suspiciousOrders = 0;

    vector<long long> ordersByCategory;
    vector<long long> ordersByStatus;
    vector<double> revenueByCategory;

    Statistics(int categoryCount = 6, int statusCount = 6) {
        ordersByCategory.assign(categoryCount, 0);
        ordersByStatus.assign(statusCount, 0);
        revenueByCategory.assign(categoryCount, 0.0);
    }

    void merge(const Statistics& other) {
        totalRevenue += other.totalRevenue;
        marketplaceCommission += other.marketplaceCommission;
        totalDeliveryCost += other.totalDeliveryCost;
        totalDiscount += other.totalDiscount;

        totalOrders += other.totalOrders;
        totalItems += other.totalItems;
        suspiciousOrders += other.suspiciousOrders;

        for (size_t i = 0; i < ordersByCategory.size(); ++i) {
            ordersByCategory[i] += other.ordersByCategory[i];
            revenueByCategory[i] += other.revenueByCategory[i];
        }

        for (size_t i = 0; i < ordersByStatus.size(); ++i) {
            ordersByStatus[i] += other.ordersByStatus[i];
        }
    }
};

// ===============================
// Названия категорий и статусов
// ===============================
const vector<string> CATEGORIES = {
    "Electronics",
    "Clothes",
    "Home",
    "Food",
    "Sport",
    "Books"
};

const vector<string> STATUSES = {
    "Created",
    "Paid",
    "Shipped",
    "Delivered",
    "Cancelled",
    "Returned"
};

// ===============================
// Расчет суммы заказа
// ===============================
double calculateOrderSum(const Order& order) {
    double baseSum = order.price * order.quantity;
    double discount = baseSum * order.discountPercent / 100.0;
    return baseSum - discount + order.deliveryCost;
}

// ===============================
// Проверка подозрительного заказа
// Условно считаем заказ подозрительным,
// если сумма большая или скидка слишком высокая
// ===============================
bool isSuspiciousOrder(const Order& order, double orderSum) {
    return orderSum > 100000.0 || order.discountPercent > 70.0;
}

// ===============================
// Дополнительная вычислительная нагрузка
// Нужна, чтобы параллелизм был заметнее.
// В реальной системе здесь могли бы быть:
// расчет рейтинга, скоринг риска, комиссия,
// логистическая модель, проверка возвратов.
// ===============================
double calculateRiskScore(const Order& order) {
    double score = 0.0;

    for (int i = 0; i < 30; ++i) {
        score += sin(order.price * 0.0001 + i) * cos(order.quantity + i);
    }

    if (order.discountPercent > 50.0) {
        score += 10.0;
    }

    if (order.statusId == 5) {
        score += 5.0;
    }

    return score;
}

// ===============================
// Обработка одного заказа
// ===============================
void processOneOrder(const Order& order, Statistics& stats) {
    const double commissionRate = 0.12; // комиссия маркетплейса 12%

    double baseSum = order.price * order.quantity;
    double discount = baseSum * order.discountPercent / 100.0;
    double orderSum = baseSum - discount + order.deliveryCost;
    double commission = orderSum * commissionRate;

    // Дополнительный расчет для имитации более сложной аналитики
    double riskScore = calculateRiskScore(order);

    stats.totalRevenue += orderSum;
    stats.marketplaceCommission += commission;
    stats.totalDeliveryCost += order.deliveryCost;
    stats.totalDiscount += discount;

    stats.totalOrders++;
    stats.totalItems += order.quantity;

    stats.ordersByCategory[order.categoryId]++;
    stats.ordersByStatus[order.statusId]++;
    stats.revenueByCategory[order.categoryId] += orderSum;

    if (isSuspiciousOrder(order, orderSum) || riskScore > 20.0) {
        stats.suspiciousOrders++;
    }
}

// ===============================
// Генерация тестовых заказов
// ===============================
vector<Order> generateOrders(int count) {
    vector<Order> orders;
    orders.reserve(count);

    mt19937 generator(42); // фиксированное зерно для повторяемости тестов

    uniform_int_distribution<int> userDistribution(1, 50000);
    uniform_int_distribution<int> productDistribution(1, 100000);
    uniform_int_distribution<int> categoryDistribution(0, static_cast<int>(CATEGORIES.size()) - 1);
    uniform_int_distribution<int> statusDistribution(0, static_cast<int>(STATUSES.size()) - 1);
    uniform_int_distribution<int> quantityDistribution(1, 10);

    uniform_real_distribution<double> priceDistribution(100.0, 50000.0);
    uniform_real_distribution<double> discountDistribution(0.0, 80.0);
    uniform_real_distribution<double> deliveryDistribution(0.0, 1500.0);

    for (int i = 0; i < count; ++i) {
        Order order;

        order.id = i + 1;
        order.userId = userDistribution(generator);
        order.productId = productDistribution(generator);
        order.categoryId = categoryDistribution(generator);
        order.price = priceDistribution(generator);
        order.quantity = quantityDistribution(generator);
        order.discountPercent = discountDistribution(generator);
        order.deliveryCost = deliveryDistribution(generator);
        order.statusId = statusDistribution(generator);

        orders.push_back(order);
    }

    return orders;
}

// ===============================
// Последовательная обработка
// ===============================
Statistics processSequential(const vector<Order>& orders) {
    Statistics stats(static_cast<int>(CATEGORIES.size()), static_cast<int>(STATUSES.size()));

    for (const Order& order : orders) {
        processOneOrder(order, stats);
    }

    return stats;
}

// ===============================
// Параллельная обработка через OpenMP
// ===============================
Statistics processParallel(const vector<Order>& orders, int threadCount) {
    Statistics globalStats(static_cast<int>(CATEGORIES.size()), static_cast<int>(STATUSES.size()));

#ifdef _OPENMP
    omp_set_num_threads(threadCount);
#endif

#pragma omp parallel
    {
        Statistics localStats(static_cast<int>(CATEGORIES.size()), static_cast<int>(STATUSES.size()));

#pragma omp for
        for (int i = 0; i < static_cast<int>(orders.size()); ++i) {
            processOneOrder(orders[i], localStats);
        }

#pragma omp critical
        {
            globalStats.merge(localStats);
        }
    }

    return globalStats;
}

// ===============================
// Замер времени выполнения функции
// ===============================
template <typename Func>
pair<Statistics, double> measureTime(Func function) {
    auto start = chrono::high_resolution_clock::now();

    Statistics result = function();

    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> duration = end - start;

    return { result, duration.count() };
}

// ===============================
// Сравнение double с погрешностью
// ===============================
bool almostEqual(double a, double b, double eps = 1.0) {
    return fabs(a - b) < eps;
}

// ===============================
// Проверка совпадения результатов
// ===============================
bool compareStatistics(const Statistics& a, const Statistics& b) {
    if (!almostEqual(a.totalRevenue, b.totalRevenue, 1.0)) return false;
    if (!almostEqual(a.marketplaceCommission, b.marketplaceCommission, 1.0)) return false;
    if (!almostEqual(a.totalDeliveryCost, b.totalDeliveryCost, 1.0)) return false;
    if (!almostEqual(a.totalDiscount, b.totalDiscount, 1.0)) return false;

    if (a.totalOrders != b.totalOrders) return false;
    if (a.totalItems != b.totalItems) return false;
    if (a.suspiciousOrders != b.suspiciousOrders) return false;

    if (a.ordersByCategory != b.ordersByCategory) return false;
    if (a.ordersByStatus != b.ordersByStatus) return false;

    for (size_t i = 0; i < a.revenueByCategory.size(); ++i) {
        if (!almostEqual(a.revenueByCategory[i], b.revenueByCategory[i], 1e-3)) {
            return false;
        }
    }

    return true;
}

// ===============================
// Вывод итоговой статистики
// ===============================
void printStatistics(const Statistics& stats) {
    cout << fixed << setprecision(2);

    cout << "\nИтоговая статистика обработки заказов:\n";
    cout << "Всего заказов: " << stats.totalOrders << endl;
    cout << "Всего товаров: " << stats.totalItems << endl;
    cout << "Общая выручка: " << stats.totalRevenue << endl;
    cout << "Комиссия маркетплейса: " << stats.marketplaceCommission << endl;
    cout << "Общая сумма скидок: " << stats.totalDiscount << endl;
    cout << "Расходы на доставку: " << stats.totalDeliveryCost << endl;
    cout << "Подозрительные заказы: " << stats.suspiciousOrders << endl;

    if (stats.totalOrders > 0) {
        cout << "Средний чек: " << stats.totalRevenue / stats.totalOrders << endl;
    }

    cout << "\nЗаказы по категориям:\n";
    for (size_t i = 0; i < CATEGORIES.size(); ++i) {
        cout << CATEGORIES[i] << ": "
             << stats.ordersByCategory[i]
             << " заказов, выручка = "
             << stats.revenueByCategory[i]
             << endl;
    }

    cout << "\nЗаказы по статусам:\n";
    for (size_t i = 0; i < STATUSES.size(); ++i) {
        cout << STATUSES[i] << ": "
             << stats.ordersByStatus[i]
             << endl;
    }
}

// ===============================
// Основная программа
// ===============================
int main() {
    setlocale(LC_ALL, "Russian");

    cout << "Курсовой проект: параллельная обработка данных в маркетплейсе\n";
    cout << "Язык: C++\n";
    cout << "Технология распараллеливания: OpenMP\n";

#ifdef _OPENMP
    cout << "OpenMP включен. Максимальное число потоков: "
         << omp_get_max_threads() << "\n";
#else
    cout << "ВНИМАНИЕ: OpenMP не включен. Компилируйте с ключом -fopenmp.\n";
#endif

    vector<int> dataSizes = {
        10000,
        50000,
        100000,
        500000,
        1000000
    };

    vector<int> threadCounts = {
        2,
        4,
        6
    };

    cout << "\nСравнение времени обработки заказов\n";
    cout << "Время указано в миллисекундах\n\n";

    cout << left << setw(15) << "Orders"
         << setw(20) << "Sequential";

    for (int threads : threadCounts) {
        string header = to_string(threads) + " threads";
        cout << setw(20) << header;
    }

    cout << "\n";

    cout << string(15 + 20 * (1 + threadCounts.size()), '-') << "\n";

    Statistics lastSequentialStats;
    Statistics lastParallelStats;

    for (int size : dataSizes) {
        vector<Order> orders = generateOrders(size);

        auto sequentialResult = measureTime([&orders]() {
            return processSequential(orders);
        });

        Statistics sequentialStats = sequentialResult.first;
        double sequentialTime = sequentialResult.second;

        cout << left << setw(15) << size
             << setw(20) << fixed << setprecision(2) << sequentialTime;

        for (int threads : threadCounts) {
            auto parallelResult = measureTime([&orders, threads]() {
                return processParallel(orders, threads);
            });

            Statistics parallelStats = parallelResult.first;
            double parallelTime = parallelResult.second;

            bool correct = compareStatistics(sequentialStats, parallelStats);

            if (!correct) {
                cout << setw(20) << "Ошибка";
            } else {
                cout << setw(20) << fixed << setprecision(2) << parallelTime;
            }

            lastParallelStats = parallelStats;
        }

        cout << "\n";

        lastSequentialStats = sequentialStats;
    }

    cout << "\nПроверка последнего теста: ";
    if (compareStatistics(lastSequentialStats, lastParallelStats)) {
        cout << "результаты последовательной и параллельной обработки совпадают.\n";
    } else {
        cout << "обнаружено расхождение результатов.\n";
    }

    printStatistics(lastSequentialStats);

    cout << "\nПрограмма завершена.\n";

    return 0;
}