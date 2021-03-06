/*************************************************************************************************
    begin                : Sun Dec 28 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net
                               2005-2007 by Holger Danielsson (holger.danielsson@versanet.de)
                               2008-2010 by Michel Ludwig (michel.ludwig@kdemail.net)
 *************************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// 2005-11-02: dani
//  - cleaning up source (local variables etc.)
//  - added different QToolTips for each item
//  - add more types of KilelistViewItems
//  - KileStruct::Sect and KileStruct::BeginFloat will remember assigned labels,
//    which are displayed as QToolTips, if these labels are defined in the
//    same or the next line
//  - Caption for type KileStruct::BeginFloat are displayed in the title
//    of this item
//  - \includegraphics and float environment items are displayed
//  - if an item has a companion label, you can use the context menu (right mouse)
//    to insert this label as reference, as a page reference or only the keyword
//    into the text or copy it to the clipboard.
//  - graphics files have also a context menu to open them with a special program

// 2005-12-08: dani
//  - make some items like labels, bibitems, graphics and float environments
//    configurable for the user

// 2005-12-16: dani
//  - add listview item for undefined references

// 2007-02-15: dani
// - class StructureViewItem gets two new members to
//   save the real cursor position of the command

// 2007-03-12 dani
//  - use KileDocument::Extensions

// 2007-03-17 dani
//  - remember how document structure is collapsed, when structure view is refreshed

// 2007-03-24: dani
// - preliminary minimal support for Beamer class
// - \begin{frame}...\end{frame} and \frame{...} are shown in the structure view
// - if a \frametitle command follows as next LaTeX command, its parameter
//   is taken to replace the standard title of this entry in the structure view
// - \begin{block}...\end{block} is taken as child of a frame

// 2007-04-06 dani
// - add TODO/FIXME section to structure view

#include "widgets/structurewidget.h"

#include <QClipboard>
#include <QFileInfo>
#include <QHeaderView>
#include <QRegExp>
#include <QScrollBar>
#include <QSignalMapper>

#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KMimeType>
#include <KRun>
#include <KUrl>

#include "kileconfig.h"
#include "kiledebug.h"
#include "kiledocmanager.h"
#include "kileinfo.h"
#include "documentinfo.h"
#include "kileproject.h"
#include "kiletool_enums.h"

namespace KileWidget
{
////////////////////// StructureViewItem with all info //////////////////////

StructureViewItem::StructureViewItem(QTreeWidgetItem* parent, const QString &title, const KUrl &url, uint line, uint column, int type, int level, uint startline, uint startcol) :
	QTreeWidgetItem(parent),
	m_title(title), m_url(url), m_line(line), m_column(column), m_type(type), m_level(level),
	m_startline(startline), m_startcol(startcol)
{
	setItemEntry();
}

StructureViewItem::StructureViewItem(QTreeWidget* parent, const QString& label) :
	QTreeWidgetItem(parent, QStringList(label)),
	m_title(label), m_url(KUrl()), m_line(0),  m_column(0), m_type(KileStruct::None), m_level(0)
{
	setToolTip(0, i18n("Click left to jump to the line. A double click will open\n a text file or a graphics file. When a label is assigned\nto this item, it will be shown when the mouse is over\nthis item. Items for a graphics file or an assigned label\nalso offer a context menu (right mouse button)."));
}

StructureViewItem::StructureViewItem(const QString& label, QTreeWidgetItem* parent) :
	QTreeWidgetItem(parent, QStringList(label)),
	m_title(label), m_url(KUrl()), m_line(0),  m_column(0), m_type(KileStruct::None), m_level(0)
{}

void StructureViewItem::setTitle(const QString &title)
{
	m_title = title;
	setItemEntry();
}

void StructureViewItem::setItemEntry()
{
	setText(0, i18nc("structure view entry: title (line)", "%1 (line %2)", m_title, QString::number(m_line)));
	setToolTip(0, text(0));
}

void StructureViewItem::setLabel(const QString &label)
{
	m_label = label;
	if(!m_label.isEmpty()) {
		setToolTip(0, i18n("Label: %1", m_label));
	}
}

////////////////////// StructureView tree widget //////////////////////

	StructureView::StructureView(StructureWidget *stack, KileDocument::Info *docinfo) :
		QTreeWidget(stack),
		m_stack(stack), m_docinfo(docinfo)
	{
		stack->addWidget(this);
		setColumnCount(1);
		QStringList labelList;
		labelList << i18n("Structure");
		setHeaderLabels(labelList);

		header()->hide();
		header()->setResizeMode(QHeaderView::ResizeToContents);
		setAllColumnsShowFocus(true);
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

		//connect(this, SIGNAL(clicked(QListViewItem *)), m_stack, SLOT(slotClicked(QListViewItem *)));
		connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), m_stack, SLOT(slotDoubleClicked(QTreeWidgetItem*)));

		connect(this, SIGNAL(itemClicked(QTreeWidgetItem*,int)), m_stack, SLOT(slotClicked(QTreeWidgetItem*)));
		connect(m_stack, SIGNAL(configChanged()), this, SLOT(slotConfigChanged()));

		init();
	}

	StructureView::~StructureView() {}

	void StructureView::init()
	{
		QString title = (!m_docinfo) ? i18n("No \"structure data\" to display.") : m_docinfo->url().fileName();
		m_root =  new StructureViewItem(this, title);
		if(m_docinfo) {
			m_root->setURL(m_docinfo->url());
			m_root->setExpanded(true);
			m_root->setIcon(0, KIcon("contents"));
			connect(m_docinfo, SIGNAL(foundItem(const QString&, uint, uint, int, int, uint, uint, const QString &, const QString &)),
			        this, SLOT(addItem(const QString&, uint, uint, int, int, uint, uint, const QString &, const QString &)));
// 			connect(m_docinfo, SIGNAL(doneUpdating()), this, SLOT(insertInMasterList()));
		}

		m_parent[0]=m_parent[1]=m_parent[2]=m_parent[3]=m_parent[4]=m_parent[5]=m_parent[6]=m_root;
		m_lastType = KileStruct::None;
		m_lastSectioning = NULL;
		m_lastFloat = NULL;
		m_lastFrame = NULL;
		m_lastFrameEnv = NULL;
		m_stop = false;

		m_folders.clear();
		m_references.clear();

		if(m_docinfo) {
 			m_openStructureLabels = m_docinfo->openStructureLabels();
 			m_openStructureReferences = m_docinfo->openStructureReferences();
			m_openStructureBibitems = m_docinfo->openStructureBibitems();
			m_openStructureTodo = m_docinfo->openStructureTodo();
			m_showStructureLabels = m_docinfo->showStructureLabels();
		}
		else {
			m_openStructureLabels = false;
			m_openStructureReferences = false;
			m_openStructureBibitems = false;
			m_openStructureTodo = false;
			m_showStructureLabels = false;
 		}
	}

	void StructureView::updateRoot()
	{
		m_root->setURL( m_docinfo->url() );
		m_root->setText(0, m_docinfo->url().fileName() );
	}

	void StructureView::cleanUp(bool preserveState/* = true */)
	{
        	KILE_DEBUG() << "==void StructureView::cleanUp()========";
		if(preserveState) {
			saveState();
		}
		clear();
		if(m_docinfo) {
			disconnect(m_docinfo, 0, this, 0);
		}
		init();
	}

	void StructureView::slotConfigChanged(){
		QWidget *current = m_stack->currentWidget();
		if(!current) {
			return;
		}
		cleanUp(false);
 		m_stack->update(m_docinfo, true, true);
	}

	void StructureView::contextMenuEvent(QContextMenuEvent *event)
	{
		m_stack->viewContextMenuEvent(this, event);
	}

	void StructureView::saveState()
	{
		KILE_DEBUG() << "===void StructureView::saveState()";
		m_openByTitle.clear();
		m_openByLine.clear();
		m_openByFolders.clear();

		QTreeWidgetItemIterator it(this);
		StructureViewItem *item = NULL;
		while(*it) {
			item = dynamic_cast<StructureViewItem*>(*it);
			if(item && item->child(0)) {
				//we don't accept duplicate entries in the map, remove this item alltogether
				//and rely on the openByLine map instead
				if(m_openByTitle.contains(item->title())) {
					m_openByTitle.remove(item->title());
				}
				else {
					m_openByTitle[item->title()] = item->isExpanded();
				}

				m_openByLine[item->line()] = item->isExpanded();
			}
			++it;
		}

		if(m_folders.contains("labels")) {
			m_openByFolders["labels"] = m_folders["labels"]->isExpanded();
		}
		if(m_folders.contains("refs")) {
			m_openByFolders["refs"] = m_folders["refs"]->isExpanded();
		}
		if(m_folders.contains("bibs")) {
			m_openByFolders["bibs"] = m_folders["bibs"]->isExpanded();
		}
		if(m_folders.contains("todo")) {
			m_openByFolders["todo"] = m_folders["todo"]->isExpanded();
		}
		if(m_folders.contains("fixme")) {
			m_openByFolders["fixme"] = m_folders["fixme"]->isExpanded();
		}
	}

	bool StructureView::shouldBeOpen(StructureViewItem *item, const QString & folder, int level)
	{
		if(!item->parent()) {
			return true;
		}
		if(folder == "labels") {
			if(m_openByFolders.contains("labels")) {
				return m_openByFolders["labels"];
			}
			else {
				return m_openStructureLabels;
			}
		}
		else if(folder == "refs") {
			if(m_openByFolders.contains("refs")) {
				return m_openByFolders["refs"];
			}
			else {
				return m_openStructureReferences;
			}
		}
		else if(folder == "bibs") {
			if(m_openByFolders.contains("bibs")) {
				return m_openByFolders["bibs"];
			}
			else {
				return m_openStructureBibitems;
			}
		}
		else if(folder=="todo" || folder=="fixme") {
			if(m_openByFolders.contains(folder)) {
				return m_openByFolders[folder];
			}
			else {
				return m_openStructureTodo;
			}
		}

		if(m_openByTitle.contains(item->title())) {
			return m_openByTitle[item->title()];
		}
		else if(m_openByLine.contains(item->line())) {
			return m_openByLine[item->line()]; //TODO check surrounding lines as well
		}
		else {
			return ((folder == "root") && level <= m_stack->level());
		}
	}

	StructureViewItem* StructureView::createFolder(const QString &folder)
	{
		StructureViewItem *fldr = new StructureViewItem(folder);
		// add it as a top-level child
		m_root->insertChild(0, fldr);
		fldr->setExpanded(false);
		if(folder == "labels") {
			fldr->setText(0, i18n("Labels"));
			fldr->setIcon(0, KIcon("label"));
		}
		else if(folder == "bibs") {
			fldr->setText(0, i18n("BibTeX References"));
			fldr->setIcon(0, KIcon("viewbib"));
		}
		else if(folder == "refs") {
			fldr->setText(0, i18n("Undefined References"));
			fldr->setIcon(0, KIcon("dialog-error"));
		}
		else if(folder == "todo") {
			fldr->setText(0, i18n("TODO"));
			fldr->setIcon(0, KIcon("bookmarks"));
		}
		else if(folder == "fixme") {
			fldr->setText(0, i18n("FIXME"));
			fldr->setIcon(0, KIcon("bookmarks"));
		}

		m_folders[folder] = fldr;

		return m_folders[folder];
	}

	StructureViewItem* StructureView::folder(const QString &folder)
	{
		StructureViewItem *item = m_folders[folder];
		if(!item) {
			item = createFolder(folder);
		}
		return item;
	}

	void StructureView::activate()
	{
		if(m_stack->indexOf(this) >= 0) {
			m_stack->setCurrentWidget(this);
		}
	}

	StructureViewItem *StructureView::parentFor(int lev, const QString & fldr)
	{
		StructureViewItem *par = NULL;

		if(fldr == "root") {
			switch(lev) {
				case KileStruct::Object:
				case KileStruct::File:
					par = (!m_lastSectioning) ? m_root : m_lastSectioning;
				break;

				case 0:
				case 1:
					par = m_root;
				break;

				default:
					par = m_parent[lev - 2];
				break;
			}
		}
		else {
			par = folder(fldr);
		}

		return par;
	}

////////////////////// add a new item to the tree widget //////////////////////

	/* some items have a special action:
		- KileStruct::Sect:
		      The new item is saved in m_lastSectioning, so that all following entries
		      can be inserted as children. \part will drop back to level 0 of the Listview,
		      all other sectioning commands will be children of the last sectioning item.
		      If a \label command follows in the same or the next line, it is assigned
		      to this item.
		- KileStruct::BeginFloat:
		      The new item is saved in m_lastFloat. If a \caption command follows before
		      the floating environment is closed, it is inserted into the title of this item.
		      If a \label command follows, it is assigned to this float item.
		- KileStruct::EndFloat
		      Reset m_lastFloat to NULL to close this environment. No more \caption or \label
		      commands are assigned to this float after this.
		- KileStruct::Caption
		      If a float environment is opened, the caption is assigned to the float item.
		      A caption item has hidden attribute, so that no other action is performed and
		      function addItem() will return immediately.
		- KileStruct::Label
		      If we are inside a float, this label is assigned to this environment. If the last
		      type was a sectioning command on the current line or the line before, the label is
		      assigned to this sectioning item. Assigning means that a popup menu will open,
		      when the mouse is over this item.
		- KileStruct::BeamerBeginFrame
		      The new item is saved in m_lastFrameEnv. If a \frametitle command follows before
		      the frame environment is closed, it is inserted into the title of this item.
		      If a \label command follows, it is assigned to this float item.
		- KileStruct::BeamerEndFrame
		      Reset m_lastFloatEnv to NULL to close this environment. No more \frametitle
		      or \label commands are assigned to this frame after this.
		- KileStruct::BeamerBeginBlock
		      Inside a beamer frame this environment is taken as child of this frame
		- KileStruct::BeamerFrame
		      The new item is saved in m_lastFrame. If a \frametitle command follows
		      immediately as next command, it is inserted into the title of this item.
		*/

	void StructureView::addItem(const QString &title, uint line, uint column, int type, int lev,
	                            uint startline, uint startcol,
	                            const QString &pix, const QString &fldr /* = "root" */)
	{
//  		KILE_DEBUG() << "\t\taddItem: " << title << ", with type " <<  type;
		if(m_stop) {
			return;
		}

		// some types need a special action
		if(type == KileStruct::Reference) {
			m_references.prepend(KileReferenceData(title, line, column));
		}
		else if(type==KileStruct::Caption && m_lastFloat) {
			QString floattitle = m_lastFloat->title();
			if(floattitle == "figure" || floattitle == "table") {
				m_lastFloat->setTitle(floattitle+": "+title);
			}
		}
		else if(type == KileStruct::EndFloat) {
			m_lastFloat = NULL;
		}
		else if(type == KileStruct::BeamerFrametitle) {
			if(m_lastFrameEnv) {
				m_lastFrameEnv->setTitle(title);
			}
			else if(m_lastFrame) {
				m_lastFrame->setTitle(title);
			}
		}
		else if(type == KileStruct::BeamerEndFrame) {
			m_lastFrameEnv = NULL;
		}
		m_lastFrame = NULL;

		// that's all for hidden types: we must immediately return
		if(lev == KileStruct::Hidden) {
			//KILE_DEBUG() << "\t\thidden item: not created";
			return;
		}

		//KILE_DEBUG() << "\t\tcreate new item";
		// check if we have to update a label before loosing this item
		if(type==KileStruct::Label) {
			if(m_lastFloat) {
				m_lastFloat->setLabel(title);
			}
			else if(m_lastType == KileStruct::Sect) {
				// check if this is a follow up label for the last sectioning item
				if(m_lastSectioning && (m_lastLine == line-1 || m_lastLine==line)) {
					m_lastSectioning->setLabel(title);
				}
			}
			else if(m_lastType == KileStruct::BeamerBeginFrame && m_lastFrameEnv) {
				m_lastFrameEnv->setLabel(title);
			}

			if(!m_showStructureLabels) { // if we don't want to have it displayed return here
					return;
			}
		}

		// remember current type and line for the next call of addItem()
		m_lastType = type;
		m_lastLine = line;

		//find the parent for the new element
		StructureViewItem *parentItem = (type == KileStruct::BeamerBeginBlock && m_lastFrameEnv) ? m_lastFrameEnv : parentFor(lev, fldr);
		if(!parentItem) {
			KMessageBox::error(m_stack->info()->mainWindow(), i18n("Cannot create a list view item: no parent found."));
			return;
		}

		// create a new item
		StructureViewItem *newChild = new StructureViewItem(parentItem, title, m_docinfo->url(), line, column, type, lev, startline, startcol);
		if(!pix.isEmpty()) {
			newChild->setIcon(0, KIcon(pix));
		}
		//m_stop = true;

		//if the level is not greater than the defaultLevel
		//open the parentItem to make this item visible
		parentItem->setExpanded(shouldBeOpen(parentItem, fldr, lev));

		//update the m_parent levels, such that section etc. get inserted at the correct level
		//m_current = newChild;
		if(lev > 0) {
			m_parent[lev - 1] = newChild;
			for(int l = lev; l < 7; ++l) {
				m_parent[l] = newChild;
			}
		}
		else if(lev == 0) {
			m_lastSectioning = NULL;
			for(int l = 0; l < 7; ++l) {
				m_parent[l] = m_root;
			}
		}

		// check if we have to remember the new item for setting a label or caption
		if(type == KileStruct::Sect) {
			m_lastSectioning = newChild;
		}
		else if(type == KileStruct::BeginFloat) {
			m_lastFloat = newChild;
		}
		else if(type == KileStruct::BeamerBeginFrame) {
			m_lastFrameEnv = newChild;
		}
		else if(type == KileStruct::BeamerFrame) {
			m_lastFrame = newChild;
		}
	}

	void StructureView::showReferences(KileInfo *ki)
	{
		// remove old listview item for references, if it exists
		if(m_folders.contains("refs")) {
			StructureViewItem *refitem = m_folders["refs"];
			m_root->removeChild(refitem);
			delete refitem;

			m_folders.remove("refs");
		}

		//KILE_DEBUG() << "==void StructureView::showReferences()========";
		//KILE_DEBUG() << "\tfound " << m_references.count() << " references";
		if(m_references.count() == 0) {
			return;
		}

		// read list with all labels
		QStringList list = ki->allLabels();
		//KILE_DEBUG() << "\tfound " << list.count() << " labels";
		QMap<QString,bool> labelmap;
		for (QStringList::const_iterator itmap = list.constBegin(); itmap != list.constEnd(); ++itmap) {
			labelmap[(*itmap)] = true;
		}

		// now check if there are unsolved references
		for (QList<KileReferenceData>::const_iterator it = m_references.constBegin(); it!=m_references.constEnd(); ++it) {
			if(!labelmap.contains((*it).name())) {
				StructureViewItem *refitem = folder("refs");
				refitem->setExpanded(shouldBeOpen(refitem, "refs", 0));
				new StructureViewItem(refitem, (*it).name(), m_docinfo->url(), (*it).line(), (*it).column(), KileStruct::Reference, KileStruct::NotSpecified, 0, 0);
			}
		}
	}

	////////////////////// StructureWidget: QWidgetStack //////////////////////

	StructureWidget::StructureWidget(KileInfo *ki, QWidget * parent, const char* name) :
		QStackedWidget(parent),
		m_ki(ki),
		m_docinfo(NULL)
	{
		setObjectName(name);
		KILE_DEBUG() << "==KileWidget::StructureWidget::StructureWidget()===========";
		setLineWidth(0);
		setMidLineWidth(0);
		setContentsMargins(0, 0, 0, 0);
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

		m_default = new StructureView(this, NULL);
		m_default->activate();
	}

	StructureWidget::~StructureWidget()
	{
	}

	int StructureWidget::level()
	{
		return KileConfig::defaultLevel();
	}

	void StructureWidget::addDocumentInfo(KileDocument::Info *docinfo)
	{
		StructureView *view = new StructureView(this, docinfo);
		addWidget(view);
		m_map.insert(docinfo, view);
	}

	void StructureWidget::slotClicked(QTreeWidgetItem * itm)
	{
		KILE_DEBUG() << "\tStructureWidget::slotClicked";

		StructureViewItem *item = dynamic_cast<StructureViewItem*>(itm);
		//return if user didn't click on an item
		if(!item) {
			return;
		}

		if(!(item->type() & KileStruct::None)) {
			emit(setCursor(item->url(), item->line()-1, item->column()));
		}
		else if(!item->parent()) { //root item
			emit(setCursor(item->url(), 0, 0));
		}
	}

	void StructureWidget::slotDoubleClicked(QTreeWidgetItem * itm)
	{
		KILE_DEBUG() << "\tStructureWidget::slotDoubleClicked";
		StructureViewItem *item = dynamic_cast<StructureViewItem*>(itm);
		static QRegExp suffix("\\.[\\d\\w]*$");

		if (!item) {
			return;
		}

		KILE_DEBUG() <<"item->url() is " << item->url() << ", item->title() is " << item->title();

		if(item->type() & (KileStruct::Input | KileStruct::Bibliography | KileStruct::Graphics)) {
			QString fname = item->title();


			if(fname.indexOf(suffix) != -1) { // check if we have a suffix, if not add standard suffixes
				KILE_DEBUG() << "Suffix found: " << suffix.cap(0);
			}
			else {
				// filename in structureview entry has no extension: this shouldn't happen anymore,
				// because all have got one in function updateStruct(). But who knows?
				if(item->type() == KileStruct::Input) {
					fname += m_ki->extensions()->latexDocumentDefault();
				}
				else if(item->type() == KileStruct::Bibliography) {
					fname += m_ki->extensions()->bibtexDefault();
				}
				else if(item->type() == KileStruct::Graphics){

					KileProjectItem *kiItem = m_ki->docManager()->itemFor(item->url());
					KileProject *proj;
					QString extToAdd;
					bool fromProject = true;

					if(kiItem && (proj = kiItem->project())){
						extToAdd = proj->defaultGraphicExt();
					}
					if(extToAdd.isEmpty()){
						extToAdd = KileConfig::svDefaultGraphicExt();
						fromProject = false;
					}

					m_ki->logWidget()->printMessage(KileTool::Info,
						i18n("No extension specified for graphic file.  Using .") + extToAdd +
							(fromProject ? i18n(" from Project settings") : i18n(" from global Structure View settings.")),
						i18n("File extension not specified") );

					fname += '.' + extToAdd;

				}
				else{
					KILE_DEBUG() << "Suffixless item with unknown type found";
				}

			}

			if(QDir::isRelativePath(fname)) { // no absolute path
				QString fn = m_ki->getCompileName();
				fname= QFileInfo(fn).path() + QDir::separator() + fname;
			}

			QFileInfo fi(fname);
			KUrl url;
			url.setPath(fname);

			if (fi.isReadable()) {
				if( item->type() == KileStruct::Graphics) {
					KMimeType::Ptr pMime = KMimeType::findByUrl(url);
					KRun::runUrl(url, pMime->name(), this);
				}
				else {
					emit(fileOpen(url, QString()));
				}
			}
			else{
				QString otherFilename;

				if(item->type() == KileStruct::Bibliography) {
					otherFilename = m_ki->checkOtherPaths(fi.path(), fi.fileName(), KileInfo::bibinputs);
				}
				else if(item->type() == KileStruct::Input) {
					otherFilename = m_ki->checkOtherPaths(fi.path(), fi.fileName(), KileInfo::texinputs);
				}

				fi.setFile(otherFilename);

				if(fi.isReadable()) {
					url.setPath(otherFilename);
					emit(fileOpen(url, QString()));
				}
				else {
					if(KMessageBox::warningYesNo(this, i18n("Cannot find the included file. The file does not exist, is not readable or Kile is unable to determine the correct path to it. The filename causing this error was: %1.\nDo you want to create this file?", fname), i18n("Cannot Find File"))
					   == KMessageBox::Yes) {
						url.setPath(fname);
						emit(fileNew(url));
					}
				}
			}
		}
	}

// all popup items get different id's, so that we can see, what item is activated
	//  - label:       1 -  6
	//  - sectioning: 10 - 16
	//  - graphics:   100ff
	void StructureWidget::viewContextMenuEvent(StructureView *view, QContextMenuEvent *event)
	{
		KILE_DEBUG() << "\tcalled";

		QSignalMapper signalMapper;
		connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(slotPopupActivated(int)));
		KMenu popup;
		QAction *action = NULL;

		m_popupItem = dynamic_cast<StructureViewItem*>(view->itemAt(event->pos()));
		if(!m_popupItem) {
			KILE_DEBUG() << "not a pointer to a StructureViewItem object.";
			return;
		}

		bool hasLabel = !(m_popupItem->label().isEmpty());

		if(m_popupItem->type() == KileStruct::Sect) {
			if(hasLabel) {
				popup.addTitle(i18n("Sectioning"));
			}
			action = popup.addAction(i18n("Cu&t"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningCut);
			action = popup.addAction(i18n("&Copy"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningCopy);
			action = popup.addAction(i18n("&Paste below"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningPaste);
			popup.addSeparator();
			action = popup.addAction(i18n("&Select"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningSelect);
			action = popup.addAction(i18n("&Delete"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningDelete);
			popup.addSeparator();
			action = popup.addAction(i18n("C&omment"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningComment);
			popup.addSeparator();
			action = popup.addAction(i18n("Run QuickPreview"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, SectioningPreview);
		}
		else if(m_popupItem->type() == KileStruct::Graphics) {
			m_popupInfo = m_popupItem->title();

			if(!QDir::isAbsolutePath(m_popupInfo)) {
				QString fn = m_ki->getCompileName();
				m_popupInfo = QFileInfo(fn).path() + '/' + m_popupInfo;
			}

			QFileInfo fi(m_popupInfo);
			if(fi.isReadable()) {
				KUrl url;
				url.setPath(m_popupInfo);

				m_offerList = KMimeTypeTrader::self()->query(KMimeType::findByUrl(url)->name(), "Application");
				for(int i = 0; i < m_offerList.count(); ++i) {
					action = popup.addAction(SmallIcon(m_offerList[i]->icon()), m_offerList[i]->name(),
					                         &signalMapper, SLOT(map()));
					signalMapper.setMapping(action, i + SectioningGraphicsOfferlist);
				}
				popup.addSeparator();
				action = popup.addAction(i18n("Other..."), &signalMapper, SLOT(map()));
				signalMapper.setMapping(action, SectioningGraphicsOther);
			}
		}

		if(hasLabel) {
			popup.addTitle(i18n("Insert Label"));
			action = popup.addAction(i18n("As &reference"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 1);
			action = popup.addAction(i18n("As &page reference"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 2);
			action = popup.addAction(i18n("Only the &label"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 3);
			popup.addSeparator();
			popup.addTitle(i18n("Copy Label to Clipboard"));
			action = popup.addAction(i18n("As reference"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 4);
			action = popup.addAction(i18n("As page reference"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 5);
			action = popup.addAction(i18n("Only the label"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, 6);
		}

		if(!popup.isEmpty()) {
			popup.exec(event->globalPos());
		}
	}

	void StructureWidget::slotPopupActivated(int id)
	{
		KILE_DEBUG() << "id: " << id;
		if(id >= 1 && id <= 6) {
			slotPopupLabel(id);
		}
		else if(id >= SectioningCut && id <= SectioningPreview) {
			slotPopupSectioning(id);
		}
		else if(id >= SectioningGraphicsOther && id <= SectioningGraphicsOfferlist + m_offerList.count()) {
			slotPopupGraphics(id);
		}
	}

	// id's 1..6 (already checked)
	void StructureWidget::slotPopupLabel(int id)
	{
		KILE_DEBUG() << "\tStructureWidget::slotPopupLabel (" << id << ")"<< endl;

		QString s = m_popupItem->label();
		if(id == 1 || id == 4) {
			s = "\\ref{" + s + '}';
		}
		else if(id == 2 || id == 5) {
			s = "\\pageref{" + s + '}';
		}

		if(id <= 3) {
			emit(sendText(s));
		}
		else {
			QApplication::clipboard()->setText(s);
		}
 	}

	// id's 10..16 (already checked)
	void StructureWidget::slotPopupSectioning(int id)
	{
		KILE_DEBUG() << "\tStructureWidget::slotPopupSectioning (" << id << ")"<< endl;
		if(m_popupItem->level() >= 1 && m_popupItem->level() <= 7) {
			emit(sectioningPopup(m_popupItem, id));
		}
	}

	// id's 100ff (already checked)
	void StructureWidget::slotPopupGraphics(int id)
	{
		KILE_DEBUG() << "\tStructureWidget::slotPopupGraphics (" << id << ")"<< endl;

		KUrl url;
		url.setPath(m_popupInfo);

		if(id == SectioningGraphicsOther) {
			KRun::displayOpenWithDialog(url, this);
		}
		else {
			KRun::run(*m_offerList[id-SectioningGraphicsOfferlist], url, this);
		}
	}

	StructureView* StructureWidget::viewFor(KileDocument::Info *info)
	{
		if(!info) {
			return NULL;
		}

		if(!viewExistsFor(info)) {
			m_map.insert(info, new StructureView(this, info));
		}

		return m_map[info];
	}

	bool StructureWidget::viewExistsFor(KileDocument::Info *info)
	{
		if(!info) {
			return false;
		}
		else {
			return m_map.contains(info);
		}
	}

	void StructureWidget::closeDocumentInfo(KileDocument::Info *docinfo)
	{
		m_docinfo = NULL;
		if(m_map.contains(docinfo)) {
			StructureView *data = m_map[docinfo];
			m_map.remove(docinfo);
			delete data;
		}

		if(m_map.isEmpty()) {
			m_default->activate();
		}
	}

	void StructureWidget::clear()
	{
		QMap<KileDocument::Info *, StructureView *>::iterator it;
		QMap<KileDocument::Info *, StructureView *>::iterator itend(m_map.end());
		for (it = m_map.begin(); it != itend; ++it) {
			if(it.value()) {
				delete it.value();
			}
		}

		m_map.clear();
		m_docinfo = NULL;

		m_default->activate();
	}

	void StructureWidget::updateUrl(KileDocument::Info *docinfo)
	{
		StructureView *view = viewFor(docinfo);
		if(view) {
			view->updateRoot();
		}
	}

	void StructureWidget::update(KileDocument::Info *docinfo)
	{
		update(docinfo, false);
	}

	void StructureWidget::update(KileDocument::Info *docinfo, bool forceParsing, bool activate /* =true */)
	{
		KILE_DEBUG() << "==KileWidget::StructureWidget::update(" << docinfo << ")=============";

		if(!docinfo) {
			m_default->activate();
			return;
		}

		m_docinfo = docinfo;
		bool needParsing = forceParsing || m_docinfo->isDirty() || !viewExistsFor(docinfo);

		//find structview-item for this docinfo
		StructureView *view = viewFor(m_docinfo);
		if(!view) {
			m_default->activate();
			return;
		}

		if(needParsing) { //need to reparse the doc
			int xtop = view->horizontalScrollBar()->value();
			int ytop = view->verticalScrollBar()->value();
			//KILE_DEBUG() << "\tStructure::update parsing doc";
			// avoid flickering when parsing
			view->setUpdatesEnabled(false);
			view->cleanUp();
			m_docinfo->updateStruct();
			view->setUpdatesEnabled(true);
			view->showReferences(m_ki);
			view->horizontalScrollBar()->setValue(xtop);
			view->verticalScrollBar()->setValue(ytop);
		}

		if(activate) {
			KILE_DEBUG() << "===StructureWidget::update() activating view";
			view->activate();
		}
	}

	void StructureWidget::clean(KileDocument::Info *docinfo)
	{
		KILE_DEBUG() << "==void StructureWidget::clean()========";
		StructureView *view = viewFor(docinfo);
		if(view) {
			view->cleanUp();
		}
	}

	void StructureWidget::updateReferences(KileDocument::Info *docinfo)
	{
		KILE_DEBUG() << "==void StructureView::updateReferences()========";
		StructureView *view = viewFor(docinfo);
		if(view) {
			view->showReferences(m_ki);
		}
	}

	////////////////////// StructureWidget: find sectioning //////////////////////

	bool StructureWidget::findSectioning(StructureViewItem *refItem, KTextEditor::Document *doc, int row, int col, bool backwards, bool checkLevel, int &sectRow, int &sectCol)
	{
		KileDocument::TextInfo *docinfo = m_ki->docManager()->textInfoFor(doc);
		if(!docinfo) {
			return false;
		}

		if( checkLevel && !refItem ){ // only allow a refItem == NULL if checkLevel is false
			return false;
		}

		bool found = false;
		int foundRow, foundCol;
		StructureView *structurelist = viewFor(docinfo);
		QTreeWidgetItemIterator it(structurelist);
		while(*it) {
			StructureViewItem *item = dynamic_cast<StructureViewItem*>(*it);
			if  (item && item->type() == KileStruct::Sect && (!checkLevel || item->level() <= refItem->level())){
				foundRow = item->startline() - 1;
				foundCol = item->startcol() - 1;
				if(backwards) {
					if(foundRow < row || (foundRow==row && foundCol < col)) {
						sectRow = foundRow;
						sectCol = foundCol;
						found = true;
					}
					else {
						return found;
					}

				}
				else if(!backwards && (foundRow > row || (foundRow == row && foundCol > col))) {
					sectRow = foundRow;
					sectCol = foundCol;
					return true;
				}
			}
			++it;
		}

		return found;
	}

}

#include "structurewidget.moc"
