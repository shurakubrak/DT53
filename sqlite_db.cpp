#include <iostream>
#include "includes/sqlite_db.h"

using namespace std;


bool sqlite_db_t::open_db()
{
	if (m_connection)
		sqlite3_close(m_connection);
	if(sqlite3_open(m_dbname.c_str(), &m_connection))
		return false;
	return true;
}
//---------------------------------------------------------------------------

void sqlite_db_t::close_db()
{
	if (m_stmt)	{
		sqlite3_finalize(m_stmt);
		m_stmt = NULL;
	}
	if (m_connection)
		sqlite3_close(m_connection);
	m_connection = NULL;
}
//-------------------------------------------------------------------------

bool sqlite_db_t::exec_sql(bool SIU)
{
	if (m_stmt)
		sqlite3_finalize(m_stmt);
	if (sqlite3_prepare_v2(m_connection, m_sql_str.c_str(), -1, 
		&m_stmt, 0) != SQLITE_OK)
		return false;

	switch (sqlite3_step(m_stmt)) {
	case SQLITE_DONE:
		if (SIU)
			return true;
		else
			return false;
	case SQLITE_ROW:
		m_row_number = 0;
		return true;
	default:
		return false;
	}
}
//---------------------------------------------------------------------------

void sqlite_db_t::first_rec()
{
	if (sqlite3_reset(m_stmt) == SQLITE_OK)
		if (sqlite3_step(m_stmt) == SQLITE_ROW)
			m_row_number = 0;
}
//---------------------------------------------------------------------------

bool sqlite_db_t::next_rec()
{
	int r = sqlite3_step(m_stmt);
	switch (r) {
	case SQLITE_DONE:
		return false;
	case SQLITE_ROW:
		m_row_number++;
		break;
	default:
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

string sqlite_db_t::field_by_name(string field_name)
{
	string strResult = "";
	int type = 0;
	int field_number = sqlite3_column_count(m_stmt);
	if (field_number)
		for (int i = 0; i < field_number; i++)
			if (sqlite3_column_name(m_stmt, i) == field_name) {
				type = sqlite3_column_type(m_stmt, i);
				switch (type) {
				case SQLITE_INTEGER:
					strResult = to_string(sqlite3_column_int(m_stmt, i));
					break;
				case SQLITE_FLOAT:
					strResult = to_string(sqlite3_column_double(m_stmt, i));
					break;
				case SQLITE_TEXT:
					strResult = (char*)sqlite3_column_text(m_stmt, i);
					break;
				default:
					strResult = "";
				}
			}
	return strResult;
}

bool sqlite_db_t::atob(string str)
{
    if (str == "Y" || str == "y" || str == "Yes" || str == "yes" || str == "1" || str == "t" || str == "T" || str == "True" || str == "true"
        || str == "true" || str == "FALSE")
        return true;
    else
        return false;
}
