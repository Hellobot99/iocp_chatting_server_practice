#ifdef _DEBUG
#pragma comment(lib, "debug\\vs14\\mysqlcppconn.lib")
#pragma comment(lib, "hiredis.lib")
#else
#pragma comment(lib, "vs14\\mysqlcppconn.lib")
#pragma comment(lib, "hiredis.lib")
#endif