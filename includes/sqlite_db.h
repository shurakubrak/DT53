#pragma once
#include <string>
#include "sqlite/include/sqlite3.h"

class sqlite_db_t
{
private:
	sqlite3* m_connection;
	sqlite3_stmt* m_stmt;
	int m_row_number;
	int m_column_number;
	std::string m_field;

public:
	std::string m_dbname;
	std::string m_sql_str;
	

	sqlite_db_t(std::string db_name)
	{
		m_stmt = NULL;
		m_connection = NULL;
		m_dbname = db_name;
		m_sql_str = "";
		m_row_number = 0;
		m_column_number = 0;
	};
	~sqlite_db_t() {}

	bool open_db();
	void close_db();
	bool exec_sql(bool SIU = false);
	void first_rec();
	bool next_rec();
	void last_rec();
	std::string field_by_name(std::string field_name);
    static bool atob(std::string str);
};

