/**************************************************************************************************
    begin                : Sun Dec 28 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                               2005-2007 by Holger Danielsson (holger.danielsson@versanet.de)
                               2008-2010 by Michel Ludwig (michel.ludwig@kdemail.net)
 **************************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
  ***************************************************************************/

#ifndef STRUCTUREWIDGET_H
#define STRUCTUREWIDGET_H

#include <QList>
#include <QStackedWidget>
#include <QToolTip>
#include <QTreeWidget>

#include <KMenu>
#include <KMimeTypeTrader>
#include <KService>

#include "documentinfo.h"

//2007-02-15: dani
// - class StructureViewItem not only saves the cursor position of the parameter,
//   but also the real cursor position of the command

class KileInfo;

/**
 * ListView items that can hold some additional information appropriate for the Structure View. The
 * additional information is: line number, title string.
 **/
namespace KileWidget
{

class StructureViewItem : public QTreeWidgetItem
{
public:
	StructureViewItem(QTreeWidgetItem *parent, const QString &title, const KUrl &url, uint line, uint m_column, int type, int level, uint startline, uint startcol);
	StructureViewItem(QTreeWidget *parent, const QString &label);
	explicit StructureViewItem(const QString &label, QTreeWidgetItem *parent = NULL);

	/** @returns the title of this element (for a label it return the label), without the (line ...) part **/
	const QString& title() const { return m_title; }
	/** @returns the line number of the structure element. **/
	uint line() const { return m_line; }
	/** @returns the column number of the structure element, right after the { **/
	uint column() const { return m_column; }
	/** @returns the type of element, see @ref KileStruct **/
	int type() const { return m_type; }
	uint startline() const { return m_startline; }
	uint startcol() const { return m_startcol; }
	/**@returns the file in which this item was found*/
	const KUrl & url() const { return m_url; }
	void setURL(const KUrl & url) { m_url = url; }

	int level() const { return m_level; }
	const QString &label() const { return m_label; }
	
	void setTitle(const QString &title);
	void setLabel(const QString &label);

private:
	QString  m_title;
	KUrl     m_url;
	uint     m_line;
	uint     m_column;
	int      m_type, m_level;
	uint     m_startline;
	uint     m_startcol;
	QString  m_label;
	
	void setItemEntry();
};

class KileReferenceData
{
public:
	KileReferenceData() {}
	KileReferenceData(const QString &name, uint line, uint column) : m_name(name), m_line(line), m_column(column) {}
	~KileReferenceData() {}
	
	const QString &name() const { return m_name; }
	uint line() const { return m_line; }
	uint column() const { return m_column; }
	
private:
	QString m_name;
	uint m_line;
	uint m_column;
};

	class StructureWidget; //forward declaration

	class StructureView : public QTreeWidget
	{
		Q_OBJECT

	public:
		StructureView(StructureWidget *stack, KileDocument::Info *docinfo);
		~StructureView();

		void activate();
		void cleanUp(bool preserveState = true);
		void showReferences(KileInfo *ki);
		
		KUrl url() const { return m_docinfo->url(); }
		void updateRoot();

	public Q_SLOTS:
		void addItem(const QString &title, uint line, uint column, int type, int level, uint startline, uint startcol,
		             const QString &pix, const QString &folder = "root");
		void slotConfigChanged();

	protected:
		virtual void contextMenuEvent(QContextMenuEvent *event);

	private:
		StructureViewItem* parentFor(int lev, const QString &fldr);

		void init();
		StructureViewItem* createFolder(const QString &folder);
		StructureViewItem* folder(const QString &folder);

		void saveState();
		bool shouldBeOpen(StructureViewItem *item, const QString &folder, int level);

	private:
		StructureWidget				*m_stack;
		KileDocument::Info			*m_docinfo;
		QMap<QString, StructureViewItem *>	m_folders;
		QMap<QString, bool>			m_openByTitle;
		QMap<uint, bool>			m_openByLine;
		QMap<QString, bool>			m_openByFolders;
		StructureViewItem			*m_parent[7], *m_root;
		QList<KileReferenceData> m_references;
		bool m_openStructureLabels;
		bool m_openStructureReferences;
		bool m_openStructureBibitems;
		bool m_openStructureTodo;
		bool m_showStructureLabels;

		int m_lastType;
		uint m_lastLine;
		StructureViewItem *m_lastSectioning;
		StructureViewItem *m_lastFloat;
		StructureViewItem *m_lastFrame;
		StructureViewItem *m_lastFrameEnv;
		
		bool m_stop;
	};

	class StructureWidget : public QStackedWidget
	{
		friend class StructureView;

		Q_OBJECT

		public:
			StructureWidget(KileInfo*, QWidget *parent, const char *name = NULL);
			~StructureWidget();

			int level();
			KileInfo *info() { return m_ki; }

			bool findSectioning(StructureViewItem *item, KTextEditor::Document *doc, int row,
			                    int col, bool backwards, bool checkLevel, int &sectRow, int &sectCol);
			void updateUrl(KileDocument::Info *docinfo);

		enum { SectioningCut = 10, SectioningCopy = 11, SectioningPaste = 12, 
		       SectioningSelect = 13, SectioningDelete = 14, 
		       SectioningComment = 15,
		       SectioningPreview = 16,
		       SectioningGraphicsOther = 100, SectioningGraphicsOfferlist = 101
		     };

		public Q_SLOTS:
			void slotClicked(QTreeWidgetItem *);
			void slotDoubleClicked(QTreeWidgetItem *);
			void slotPopupActivated(int id);

			void addDocumentInfo(KileDocument::Info *);
			void closeDocumentInfo(KileDocument::Info *);
			void update(KileDocument::Info *);
			void update(KileDocument::Info *, bool, bool activate = true);
			void clean(KileDocument::Info *);
			void updateReferences(KileDocument::Info *);

			/**
			* Clears the structure widget and empties the map between KileDocument::Info objects and their structure trees (QListViewItem).
			**/
			void clear();

		Q_SIGNALS:
			void sendText(const QString&);
			void setCursor(const KUrl&, int, int);
			void fileOpen(const KUrl&, const QString&);
			void fileNew(const KUrl&);
			void configChanged();
			void sectioningPopup(KileWidget::StructureViewItem *item, int id);

		protected:
			void viewContextMenuEvent(StructureView *view, QContextMenuEvent *event);

		private:
			StructureView* viewFor(KileDocument::Info *info);
			bool viewExistsFor(KileDocument::Info *info);

			void slotPopupLabel(int id);
			void slotPopupSectioning(int id);
			void slotPopupGraphics(int id);

		private:
			KileInfo							*m_ki;
			KileDocument::Info						*m_docinfo;
			QMap<KileDocument::Info *, StructureView*>			m_map;
			StructureView							*m_default;

			StructureViewItem *m_popupItem;
			QString m_popupInfo;
			
			KService::List m_offerList;
	};
}

#endif