#include "diveimportedmodel.h"
#include "core/helpers.h"

DiveImportedModel::DiveImportedModel(QObject *o) : QAbstractTableModel(o),
	firstIndex(0),
	lastIndex(-1),
	checkStates(nullptr),
	diveTable(nullptr)
{
	// Defaults to downloadTable, can be changed later.
	diveTable = &downloadTable;
}

int DiveImportedModel::columnCount(const QModelIndex &model) const
{
	Q_UNUSED(model)
	return 3;
}

int DiveImportedModel::rowCount(const QModelIndex &model) const
{
	Q_UNUSED(model)
	return lastIndex - firstIndex + 1;
}

QVariant DiveImportedModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Vertical)
		return QVariant();

	// widgets access the model via index.column(), qml via role.
	int column = section;
	if (role == DateTime || role == Duration || role == Depth) {
		column = role - DateTime;
		role = Qt::DisplayRole;
	}

	if (role == Qt::DisplayRole) {
		switch (column) {
		case 0:
			return QVariant(tr("Date/time"));
		case 1:
			return QVariant(tr("Duration"));
		case 2:
			return QVariant(tr("Depth"));
		}
	}
	return QVariant();
}

void DiveImportedModel::setDiveTable(struct dive_table* table)
{
	diveTable = table;
}

QVariant DiveImportedModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (index.row() + firstIndex > lastIndex)
		return QVariant();

	struct dive *d = get_dive_from_table(index.row() + firstIndex, diveTable);
	if (!d)
		return QVariant();

	// widgets access the model via index.column(), qml via role.
	int column = index.column();
	if (role >= DateTime) {
		column = role - DateTime;
		role = Qt::DisplayRole;
	}

	if (role == Qt::DisplayRole) {
		switch (column) {
		case 0:
			return QVariant(get_short_dive_date_string(d->when));
		case 1:
			return QVariant(get_dive_duration_string(d->duration.seconds, tr("h"), tr("min")));
		case 2:
			return QVariant(get_depth_string(d->maxdepth.mm, true, false));
		case 3:
			return checkStates[index.row()];
		}
	}
	if (role == Qt::CheckStateRole) {
		if (index.column() == 0)
			return checkStates[index.row()] ? Qt::Checked : Qt::Unchecked;
	}
	return QVariant();
}

void DiveImportedModel::changeSelected(QModelIndex clickedIndex)
{
	checkStates[clickedIndex.row()] = !checkStates[clickedIndex.row()];
	dataChanged(index(clickedIndex.row(), 0), index(clickedIndex.row(), 0), QVector<int>() << Qt::CheckStateRole << Selected);
}

void DiveImportedModel::selectAll()
{
	memset(checkStates, true, lastIndex - firstIndex + 1);
	dataChanged(index(0, 0), index(lastIndex - firstIndex, 0), QVector<int>() << Qt::CheckStateRole << Selected);
}

void DiveImportedModel::selectRow(int row)
{
	checkStates[row] = !checkStates[row];
	dataChanged(index(row, 0), index(row, 0), QVector<int>() << Qt::CheckStateRole << Selected);
}

void DiveImportedModel::selectNone()
{
	memset(checkStates, false, lastIndex - firstIndex + 1);
	dataChanged(index(0, 0), index(lastIndex - firstIndex,0 ), QVector<int>() << Qt::CheckStateRole << Selected);
}

Qt::ItemFlags DiveImportedModel::flags(const QModelIndex &index) const
{
	if (index.column() != 0)
		return QAbstractTableModel::flags(index);
	return QAbstractTableModel::flags(index) | Qt::ItemIsUserCheckable;
}

void DiveImportedModel::clearTable()
{
	if (lastIndex < firstIndex) {
		// just to be sure it's the right numbers
		// but don't call RemoveRows or Qt in debug mode with trigger an ASSERT
		lastIndex = -1;
		firstIndex = 0;
		return;
	}
	beginRemoveRows(QModelIndex(), 0, lastIndex - firstIndex);
	lastIndex = -1;
	firstIndex = 0;
	endRemoveRows();
}

void DiveImportedModel::setImportedDivesIndexes(int first, int last)
{
	if (lastIndex >= firstIndex) {
		beginRemoveRows(QModelIndex(), 0, lastIndex - firstIndex);
		endRemoveRows();
	}
	if (last >= first)
		beginInsertRows(QModelIndex(), 0, last - first);
	lastIndex = last;
	firstIndex = first;
	delete[] checkStates;
	checkStates = new bool[last - first + 1];
	memset(checkStates, true, last - first + 1);
	if (last >= first)
		endInsertRows();
}

void DiveImportedModel::repopulate()
{
	setImportedDivesIndexes(0, diveTable->nr-1);
}

void DiveImportedModel::recordDives()
{
	if (diveTable->nr == 0)
		// nothing to do, just exit
		return;

	// walk the table of imported dives and record the ones that the user picked
	// clearing out the table as we go
	for (int i = 0; i < rowCount(); i++) {
		struct dive *d = diveTable->dives[i];
		if (d && checkStates[i]) {
			record_dive(d);
		} else {
			// we should free the dives that weren't recorded
			clear_dive(d);
			free(d);
		}
		diveTable->dives[i] = NULL;
	}
	diveTable->nr = 0;
	process_dives(true, true);
	if (autogroup)
		autogroup_dives();
}

QHash<int, QByteArray> DiveImportedModel::roleNames() const {
	static QHash<int, QByteArray> roles = {
		{ DateTime, "datetime"},
		{ Depth, "depth"},
		{ Duration, "duration"},
		{ Selected, "selected"}
	};
	return roles;
}
