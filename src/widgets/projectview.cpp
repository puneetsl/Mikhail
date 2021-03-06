/****************************************************************************************
    begin                : Tue Aug 12 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                               2006 - 2010 by Michel Ludwig (michel.ludwig@kdemail.net)
 ****************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "widgets/projectview.h"

#include <QHeaderView>
#include <QList>
#include <QSignalMapper>

#include <KActionMenu>
#include <KIcon>
#include <KLocale>
#include <KMenu>
#include <KMimeType>
#include <KMimeTypeTrader>
#include <KRun>
#include <KUrl>

#include "kileinfo.h"
#include "documentinfo.h"
#include "kiledocmanager.h"
#include <iostream>
#include <typeinfo>

const int KPV_ID_OPEN = 0, KPV_ID_SAVE = 1, KPV_ID_CLOSE = 2,
	KPV_ID_OPTIONS = 3, KPV_ID_ADD = 4, KPV_ID_REMOVE = 5,
	KPV_ID_BUILDTREE = 6, KPV_ID_ARCHIVE = 7, KPV_ID_ADDFILES = 8,
	KPV_ID_INCLUDE = 9, KPV_ID_OPENWITH = 10, KPV_ID_OPENALLFILES = 11;

namespace KileWidget {

/*
 * ProjectViewItem
 */
ProjectViewItem::ProjectViewItem(QTreeWidget *parent, KileProjectItem *item, bool ar)
: QTreeWidgetItem(parent, QStringList(item->url().fileName())), m_docinfo(NULL), m_folder(-1), m_projectItem(item)
{
	setArchiveState(ar);
}

ProjectViewItem::ProjectViewItem(QTreeWidget *parent, QTreeWidgetItem *after, KileProjectItem *item, bool ar)
: QTreeWidgetItem(parent, after), m_docinfo(NULL), m_folder(-1), m_projectItem(item)
{
	setText(0, item->url().fileName());
	setArchiveState(ar);
}

ProjectViewItem::ProjectViewItem(QTreeWidgetItem *parent, KileProjectItem *item, bool ar)
: QTreeWidgetItem(parent, QStringList(item->url().fileName())), m_docinfo(NULL), m_folder(-1), m_projectItem(item)
{
	setArchiveState(ar);
}

//use this to create folders
ProjectViewItem::ProjectViewItem(QTreeWidgetItem *parent, const QString& name)
: QTreeWidgetItem(parent, QStringList(name)), m_docinfo(NULL), m_folder(-1), m_projectItem(NULL)
{
}

//use this to create non-project files
ProjectViewItem::ProjectViewItem(QTreeWidget *parent, const QString& name)
: QTreeWidgetItem(parent, QStringList(name)), m_docinfo(NULL), m_folder(-1), m_projectItem(NULL)
{
}

ProjectViewItem::ProjectViewItem(QTreeWidget *parent, const KileProject *project)
: QTreeWidgetItem(parent, QStringList(project->name())), m_docinfo(NULL), m_folder(-1), m_projectItem(NULL)
{
}

ProjectViewItem::~ProjectViewItem()
{
	KILE_DEBUG() << "DELETING PROJVIEWITEM " << m_url.fileName();
}

KileProjectItem* ProjectViewItem::projectItem()
{
	return m_projectItem;
}

ProjectViewItem* ProjectViewItem::parent()
{
	return dynamic_cast<ProjectViewItem*>(QTreeWidgetItem::parent());
}

ProjectViewItem* ProjectViewItem::firstChild()
{
	return dynamic_cast<ProjectViewItem*>(QTreeWidgetItem::child(0));
}

void ProjectViewItem::setInfo(KileDocument::Info *docinfo)
{
	m_docinfo = docinfo;
}

KileDocument::Info* ProjectViewItem::getInfo()
{
	return m_docinfo;
}

void ProjectViewItem::setType(KileType::ProjectView type)
{
	m_type = type;
}

KileType::ProjectView ProjectViewItem::type() const
{
	return m_type;
}

void ProjectViewItem::urlChanged(const KUrl &url)
{
	// don't allow empty URLs
	if(!url.isEmpty()) {
		setURL(url);
		setText(0, url.fileName());
	}
}

void ProjectViewItem::nameChanged(const QString & name)
{
	setText(0, name);
}

void ProjectViewItem::isrootChanged(bool isroot)
{
	KILE_DEBUG() << "SLOT isrootChanged " << text(0) << " to " << isroot;
	if(isroot) {
		setIcon(0, KIcon("masteritem"));
	}
	else {
		if(text(0).right(7) == ".kilepr") {
			setIcon(0, KIcon("kile"));
		}
		else if(type() == KileType::ProjectItem) {
			setIcon(0, KIcon("projectitem"));
		}
		else {
			setIcon(0, KIcon("file"));
		}
	}
}

void ProjectViewItem::slotURLChanged(KileDocument::Info*, const KUrl & url)
{
	urlChanged(url);
}

bool ProjectViewItem::operator<(const QTreeWidgetItem& other) const
{
	try {
		const ProjectViewItem& otherItem = dynamic_cast<const ProjectViewItem&>(other);
	
		// sort:
		//  - first:  container items in fixed order (projectfile, packages, images, other)
		//  - second: root items without container (sorted in ascending order)
		if(otherItem.type() == KileType::Folder) {
			if(otherItem.type() != KileType::Folder) {
				return true;
			}
			else {
				return (m_folder < otherItem.folder()) ? false : true;
			}
		}
		else if(type() == KileType::Folder) {
			return false;
		}
		else {
			return QTreeWidgetItem::operator<(other);
		}
	}
	catch(std::bad_cast&) {
		return QTreeWidgetItem::operator<(other);
	}
}

void ProjectViewItem::setURL(const KUrl& url)
{
	m_url = url;
}

const KUrl& ProjectViewItem::url()
{
	return m_url;
}

void ProjectViewItem::setArchiveState(bool ar)
{
	setText(1, ar ? "*" : "");
}

void ProjectViewItem::setFolder(int folder)
{
	m_folder = folder;
}

int ProjectViewItem::folder() const
{
	return m_folder;
}

/*
 * ProjectView
 */
ProjectView::ProjectView(QWidget *parent, KileInfo *ki) : QTreeWidget(parent), m_ki(ki), m_nProjects(0)
{
	setColumnCount(2);
	QStringList labelList;
	labelList << i18n("Files & Projects") << i18n("Include in Archive");
	setHeaderLabels(labelList);
	setColumnWidth(1, 10);

	setFocusPolicy(Qt::ClickFocus);
	header()->hide();
	header()->setResizeMode(QHeaderView::ResizeToContents);
	setRootIsDecorated(true);
	setAllColumnsShowFocus(true);
	setSelectionMode(QTreeWidget::SingleSelection);

	connect(this, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(slotClicked(QTreeWidgetItem*)));
	setAcceptDrops(true);
}

void ProjectView::slotClicked(QTreeWidgetItem *item)
{
	if(!item) {
		item = currentItem();
	}

	ProjectViewItem *itm = static_cast<ProjectViewItem*>(item);
	if(itm) {
		if(itm->type() == KileType::File) {
			emit(fileSelected(itm->url()));
		}
		else if(itm->type() == KileType::ProjectItem) {
			emit(fileSelected(itm->projectItem()));
		}
		else if(itm->type() != KileType::Folder) {
			// don't open project configuration files (*.kilepr)
			if(itm->url().toLocalFile().right(7) != ".kilepr") {
				//determine mimeType and open file with preferred application
				KMimeType::Ptr pMime = KMimeType::findByUrl(itm->url());
				if(pMime->name().startsWith("text/")) {
					emit(fileSelected(itm->url()));
				}
				else {
					KRun::runUrl(itm->url(), pMime->name(), this);
				}
			}
		}
		clearSelection();
	}
}

void ProjectView::slotFile(int id)
{
	ProjectViewItem *item = dynamic_cast<ProjectViewItem*>(currentItem());
	if(item) {
		if(item->type() == KileType::File) {
			switch(id) {
				case KPV_ID_OPEN: 
					emit(fileSelected(item->url()));
				break;
				case KPV_ID_SAVE:
					emit(saveURL(item->url()));
				break;
				case KPV_ID_ADD:
					emit(addToProject(item->url()));
				break;
				case KPV_ID_CLOSE:
					emit(closeURL(item->url()));
				return; //don't access "item" later on
				default:
				break;
			}
		}
	}
}

void ProjectView::slotProjectItem(int id)
{
	ProjectViewItem *item = dynamic_cast<ProjectViewItem*>(currentItem());
	if(item) {
		if(item->type() == KileType::ProjectItem || item->type() == KileType::ProjectExtra) {
			switch(id) {
				case KPV_ID_OPEN:
					emit(fileSelected(item->projectItem()));
				break;
				case KPV_ID_SAVE:
					emit(saveURL(item->url()));
				break;
				case KPV_ID_REMOVE:
					emit(removeFromProject(item->projectItem()));
				break;
				case KPV_ID_INCLUDE :
					if(item->text(1) == "*") {
						item->setText(1, "");
					}
					else {
						item->setText(1, "*");
					}
					emit(toggleArchive(item->projectItem()));
				break;
				case KPV_ID_CLOSE:
					emit(closeURL(item->url()));
				break; //we can access "item" later as it isn't deleted
				case KPV_ID_OPENWITH:
					KRun::displayOpenWithDialog(item->url(), this);
				break;
				default:
				break;
			}
		}
	}
}

void ProjectView::slotProject(int id)
{
	ProjectViewItem *item = dynamic_cast<ProjectViewItem*>(currentItem());
	if(item) {
		if(item->type() == KileType::Project) {
			switch(id) {
				case KPV_ID_BUILDTREE:
					emit(buildProjectTree(item->url()));
				break;
				case KPV_ID_OPTIONS:
					emit(projectOptions(item->url()));
				break;
				case KPV_ID_CLOSE:
					emit(closeProject(item->url()));
				return; //don't access "item" later on
				case KPV_ID_ARCHIVE:
					emit(projectArchive(item->url()));
				break;
				case KPV_ID_ADDFILES:
					emit(addFiles(item->url()));
				break;
				case KPV_ID_OPENALLFILES:
					emit(openAllFiles(item->url()));
				break;
				default:
				break;
			}
		}
	}
}

void ProjectView::slotRun(int id)
{
	ProjectViewItem *itm = dynamic_cast<ProjectViewItem*>(currentItem());

	if(!itm) {
		return;
	}

	if(id == 0) {
		KRun::displayOpenWithDialog(itm->url(), this);
	}
	else {
		KRun::run(*m_offerList[id-1], itm->url(), this);
	}

	itm->setSelected(false);
}

void ProjectView::makeTheConnection(ProjectViewItem *item, KileDocument::TextInfo *textInfo)
{
	KILE_DEBUG() << "\tmakeTheConnection " << item->text(0);

	if (item->type() == KileType::Project) {
		KileProject *project = m_ki->docManager()->projectFor(item->url());
		if (!project) {
			kWarning() << "makeTheConnection COULD NOT FIND AN PROJECT OBJECT FOR " << item->url().toLocalFile();
		}
		else {
			connect(project, SIGNAL(nameChanged(const QString &)), item, SLOT(nameChanged(const QString &)));
		}
	}
	else {
		if(!textInfo) {
			textInfo = m_ki->docManager()->textInfoFor(item->url().toLocalFile());
			if(!textInfo) {
				KILE_DEBUG() << "\tmakeTheConnection COULD NOT FIND A DOCINFO";
				return;
			}
		}
		item->setInfo(textInfo);
		connect(textInfo, SIGNAL(urlChanged(KileDocument::Info*, const KUrl&)),  item, SLOT(slotURLChanged(KileDocument::Info*, const KUrl&)));
		connect(textInfo, SIGNAL(isrootChanged(bool)), item, SLOT(isrootChanged(bool)));
		//set the pixmap
		item->isrootChanged(textInfo->isLaTeXRoot());
	}
}

ProjectViewItem* ProjectView::folder(const KileProjectItem *pi, ProjectViewItem *item)
{
	ProjectViewItem *parent = parentFor(pi, item);

	if(!parent) {
		kError() << "no parent for " << pi->url().toLocalFile();
		return NULL;
	}

	// we have already found the parent folder
	if(parent->type() == KileType::Folder) {
		return parent;
	}

	// we are looking at the children, if there is an existing folder for this type
	ProjectViewItem *folder;

	// determine the foldername for this type
	QString foldername;
	switch(pi->type()) {
		case (KileProjectItem::ProjectFile):
			foldername = i18n("projectfile");
		break;
		case (KileProjectItem::Package):
			foldername = i18n("packages");
		break;
		case (KileProjectItem::Image):
			foldername = i18n("images");
		break;
		case (KileProjectItem::Other):
		default :
			foldername = i18n("other");
		break;
	}

	// if there already a folder for this type on this level?
	bool found = false;
	QTreeWidgetItemIterator it(parent);
	++it; // skip 'parent'
	while(*it) {
		folder = dynamic_cast<ProjectViewItem*>(*it);
		if(folder && folder->text(0) == foldername) {
			found = true;
			break;
		}
		++it;
	}
	
	// if no folder was found, we must create a new one
	if(!found) {
		folder = new ProjectViewItem(parent, foldername);
		KILE_DEBUG() << "new folder: parent=" << parent->url().url()
		             << ", foldername=" << foldername;

		folder->setFolder(pi->type());
		folder->setType(KileType::Folder);
	}

	return folder;
}

void ProjectView::add(const KileProject *project)
{
	ProjectViewItem *parent = new ProjectViewItem(this, project);
	
	parent->setType(KileType::Project);
	parent->setURL(project->url());
	parent->setExpanded(true);
	parent->setIcon(0, KIcon("relation"));
	makeTheConnection(parent);

	//ProjectViewItem *nonsrc = new ProjectViewItem(parent, i18n("non-source"));
	//parent->setNonSrc(nonsrc);

	refreshProjectTree(project);

	++m_nProjects;
}

ProjectViewItem* ProjectView::projectViewItemFor(const KUrl& url)
{
	ProjectViewItem *item = NULL;

	//find project view item
	QTreeWidgetItemIterator it(this);
	while(*it) {
		item = dynamic_cast<ProjectViewItem*>(*it);
		if(item && (item->type() == KileType::Project) && (item->url() == url)) {
			break;
		}
		++it;
	}

	return item;
}

ProjectViewItem* ProjectView::itemFor(const KUrl & url)
{
	ProjectViewItem *item = NULL;

	QTreeWidgetItemIterator it(this);
	while(*it) {
		item = static_cast<ProjectViewItem*>(*it);
		if (item->url() == url) {
			break;
		}
		++it;
	}

	return item;
}

ProjectViewItem* ProjectView::parentFor(const KileProjectItem *projitem, ProjectViewItem *projvi)
{
	//find parent projectviewitem of projitem
	KileProjectItem *parpi = projitem->parent();
	ProjectViewItem *parpvi = projvi, *vi;

	if (parpi) {
		//find parent viewitem that has an URL parpi->url()
		QTreeWidgetItemIterator it(projvi);
		KILE_DEBUG() << "\tlooking for " << parpi->url().toLocalFile();
		while(*it) {
			vi = static_cast<ProjectViewItem*>(*it);
			KILE_DEBUG() << "\t\t" << vi->url().toLocalFile();
			if (vi->url() == parpi->url()) {
				parpvi = vi;
				KILE_DEBUG() << "\t\tfound" <<endl;
				break;
			}
			++it;
		}

		KILE_DEBUG() << "\t\tnot found";
	}
	else {
		KILE_DEBUG() << "\tlooking for folder type " << projitem->type();
		QTreeWidgetItemIterator it(projvi);
		++it; // skip projvi
		while(*it) {
			ProjectViewItem *child = dynamic_cast<ProjectViewItem*>(*it);
			if(child && (child->type() == KileType::Folder) && (child->folder() == projitem->type())) {
				KILE_DEBUG() << "\t\tfound";
				parpvi = child;
				break;
			}
			++it;
		}
	}

	return (!parpvi) ? projvi : parpvi;
}

ProjectViewItem* ProjectView::add(KileProjectItem *projitem, ProjectViewItem *projvi /* = NULL */)
{
	KILE_DEBUG() << "\tprojectitem=" << projitem->path()
	             << " projvi=" << projvi;
	const KileProject *project = projitem->project();

	if (!projvi) {
		projvi = projectViewItemFor(project->url());
	}

	KILE_DEBUG() << "\tparent projectviewitem " << projvi->url().fileName();

	ProjectViewItem *item = NULL, *parent = NULL;

	switch (projitem->type()) {
	case (KileProjectItem::Source):
		item = new ProjectViewItem(projvi, projitem);
		item->setType(KileType::ProjectItem);
		item->setIcon(0, KIcon("projectitem"));
	break;
	case (KileProjectItem::Package):
		parent = folder(projitem, projvi);
		item = new ProjectViewItem(parent, projitem);
		item->setType(KileType::ProjectItem);
		item->setIcon(0, KIcon("projectitem"));
	break;
	case (KileProjectItem::ProjectFile):
	default:
		parent = folder(projitem, projvi);
		item = new ProjectViewItem(parent, projitem);
		item->setType(KileType::ProjectExtra);
		item->setIcon(0, KIcon((projitem->type()==KileProjectItem::ProjectFile) ? "kile" : "file"));
	break;
	}
	
	item->setArchiveState(projitem->archive());
	item->setURL(projitem->url());
	makeTheConnection(item, projitem->getInfo());

	projvi->sortChildren(0, Qt::AscendingOrder);
	// seems to be necessary to get a correct refreh (Qt 4.4.3)
	bool expanded = projvi->isExpanded();
	projvi->setExpanded(!expanded);
	projvi->setExpanded(expanded);
	
	return item;
}

void ProjectView::addTree(KileProjectItem *projitem, ProjectViewItem *projvi)
{
	KILE_DEBUG() << "projitem=" << projitem
	             << "projvi=" << projvi;
	ProjectViewItem * item = add(projitem, projvi);

	if(projitem->firstChild()) {
		addTree(projitem->firstChild(), item);
	}

	if (projitem->sibling()) {
		addTree(projitem->sibling(), projvi);
	}
}

void ProjectView::refreshProjectTree(const KileProject *project)
{
	KILE_DEBUG() << "\tProjectView::refreshProjectTree(" << project->name() << ")";
	ProjectViewItem *parent= projectViewItemFor(project->url());

	//clean the tree
	if(parent) {
		KILE_DEBUG() << "\tusing parent projectviewitem " << parent->url().fileName();
		parent->setFolder(-1);
		QList<QTreeWidgetItem*> children = parent->takeChildren();
		for(QList<QTreeWidgetItem*>::iterator it = children.begin();
		    it != children.end(); ++it) {
			delete(*it);
		}
	}
	else {
		return;
	}

	//create the non-sources dir
	//ProjectViewItem *nonsrc = new ProjectViewItem(parent, i18n("non-sources"));
	//parent->setNonSrc(nonsrc);

	QList<KileProjectItem*> list = project->rootItems();
	for(QList<KileProjectItem*>::iterator it = list.begin(); it != list.end(); ++it) {
		addTree(*it, parent);
	}

	parent->sortChildren(0, Qt::AscendingOrder);
	// seems to be necessary to get a correct refreh (Qt 4.4.3)
	bool expanded = parent->isExpanded();
	parent->setExpanded(!expanded);
	parent->setExpanded(expanded);
}

void ProjectView::add(const KUrl& url)
{
	KILE_DEBUG() << "\tProjectView::adding item " << url.toLocalFile();
	//check if file is already present
	QTreeWidgetItemIterator it(this);
	ProjectViewItem *item;
	while(*it) {
		item = static_cast<ProjectViewItem*>(*it);
		if((item->type() != KileType::Project) && (item->url() == url)) {
			return;
		}
		++it;
	}

	item = new ProjectViewItem(this, url.fileName());
	item->setType(KileType::File);
	item->setURL(url);
	makeTheConnection(item);
}

void ProjectView::remove(const KileProject *project)
{
	for(int i = 0; i < topLevelItemCount(); ++i) {
		ProjectViewItem *item = static_cast<ProjectViewItem*>(topLevelItem(i));

		if(item->url() == project->url()) {
			item->setParent(NULL);
			delete item;
			--m_nProjects;
			break;
		}
	}
}

/**
 * Removes a file from the projectview, does not remove project-items. Only files without a project.
 **/
void ProjectView::remove(const KUrl &url)
{
	for(int i = 0; i < topLevelItemCount(); ++i) {
		ProjectViewItem *item = dynamic_cast<ProjectViewItem*>(topLevelItem(i));

		if(item && (item->type() == KileType::File) && (item->url() == url)) {
			item->setParent(NULL);
			delete item;
			break;
		}
	}
}

void ProjectView::removeItem(const KileProjectItem *projitem, bool open)
{
	QTreeWidgetItemIterator it(this);
	ProjectViewItem *item;
	while(*it) {
		item = dynamic_cast<ProjectViewItem*>(*it);
		if(item && (item->type() == KileType::ProjectItem) && (item->projectItem() == projitem)) {
			KILE_DEBUG() << "removing projectviewitem";
			static_cast<QTreeWidgetItem*>(item->parent())->removeChild(item);
			delete item;
		}
		++it;
	}

	if(open) {
		item = new ProjectViewItem(this, projitem->url().fileName());
		item->setType(KileType::File);
		item->setURL(projitem->url());
		makeTheConnection(item);
	}

}

void ProjectView::contextMenuEvent(QContextMenuEvent *event)
{
	QSignalMapper signalMapper, serviceSignalMapper;
	KMenu popup;
	QAction *action = NULL;

	QTreeWidgetItem* treeWidgetItem = itemAt(event->pos());
	if(!treeWidgetItem) {
		return;
	}

	ProjectViewItem *projectViewItem = dynamic_cast<ProjectViewItem*>(treeWidgetItem);
	if(!projectViewItem) {
		return;
	}

	if(projectViewItem->type() == KileType::Folder) {
		return;
	}

	bool insertsep = false;
	bool isKilePrFile = false;
	if(projectViewItem->type() != KileType::Project && projectViewItem->projectItem()
	                                                && projectViewItem->projectItem()->project()) {
		isKilePrFile = projectViewItem->projectItem()->project()->url() == projectViewItem->url();
	}

	if(projectViewItem->type() == KileType::ProjectExtra && !isKilePrFile) {
		QMenu *servicesMenu = popup.addMenu(KIcon("fork"), i18n("&Open With"));
		connect(&serviceSignalMapper, SIGNAL(mapped(int)), this, SLOT(slotRun(int)));
		m_offerList = KMimeTypeTrader::self()->query(KMimeType::findByUrl(projectViewItem->url())->name(), "Application");
		for (int i = 0; i < m_offerList.count(); ++i) {
			action = new KAction(servicesMenu);
			action->setIcon(KIcon(m_offerList[i]->icon()));
			action->setText(m_offerList[i]->name());
			connect(action, SIGNAL(triggered()), &serviceSignalMapper, SLOT(map()));
			serviceSignalMapper.setMapping(action, i + 1);
			servicesMenu->addAction(action);
		}

		servicesMenu->addSeparator();
		action = servicesMenu->addAction(i18n("Other..."), &serviceSignalMapper, SLOT(map()));
		serviceSignalMapper.setMapping(action, 0);
		insertsep = true;
	}

	if (projectViewItem->type() == KileType::File || projectViewItem->type() == KileType::ProjectItem) {
		if(!m_ki->isOpen(projectViewItem->url())) {
			action = popup.addAction(KIcon("document-open"), i18n("&Open"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, KPV_ID_OPEN);
		}
		else {
			action = popup.addAction(KIcon("document-save"), i18n("&Save"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, KPV_ID_SAVE);
		}
		insertsep = true;
	}

	if(projectViewItem->type() == KileType::File) {
		if(m_nProjects > 0) {
			if(insertsep) {
				popup.addSeparator();
			}
			action = popup.addAction(KIcon("project_add"), i18n("&Add to Project"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, KPV_ID_ADD);
			insertsep = true;
		}
		connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(slotFile(int)));
	}
	else if(projectViewItem->type() == KileType::ProjectItem || projectViewItem->type() == KileType::ProjectExtra) {
		KileProjectItem *pi = projectViewItem->projectItem();
		if(pi) {
			if(insertsep) {
				popup.addSeparator();
			}
			action = popup.addAction(i18n("&Include in Archive"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, KPV_ID_INCLUDE);
			action->setCheckable(true);
			action->setChecked(pi->archive());
			insertsep = true;
		}
		if(!isKilePrFile) {
			if(insertsep) {
				popup.addSeparator();
			}
			action = popup.addAction(KIcon("project_remove"),i18n("&Remove From Project"), &signalMapper, SLOT(map()));
			signalMapper.setMapping(action, KPV_ID_REMOVE);
			insertsep = true;
		}
		connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(slotProjectItem(int)));
	}
	else if(projectViewItem->type() == KileType::Project) {
		if(insertsep) {
			popup.addSeparator();
		}
		action = popup.addAction(i18n("A&dd Files..."), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_ADDFILES);
		popup.addSeparator();
		action = popup.addAction(i18n("Open All &Project Files"), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_OPENALLFILES);
		popup.addSeparator();
		action = popup.addAction(KIcon("view-refresh"),i18n("Refresh Project &Tree"), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_BUILDTREE);
		action = popup.addAction(KIcon("configure"), i18n("Project &Options"), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_OPTIONS);
		action = popup.addAction(i18n("&Archive"), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_ARCHIVE);
		connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(slotProject(int)));
		insertsep = true;
	}

	if((projectViewItem->type() == KileType::File) || (projectViewItem->type() == KileType::ProjectItem)
	                                               || (projectViewItem->type()== KileType::Project)) {
		if(insertsep) {
			popup.addSeparator();
		}
		action = popup.addAction(KIcon("view-close"), i18n("&Close"), &signalMapper, SLOT(map()));
		signalMapper.setMapping(action, KPV_ID_CLOSE);
	}

	popup.exec(event->globalPos());
	m_offerList.clear();
}

void ProjectView::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasUrls()) { // only accept URL drags
		event->acceptProposedAction();
	}
}

void ProjectView::dragMoveEvent(QDragMoveEvent *event)
{
	if(event->mimeData()->hasUrls()) { // only accept URL drags
		event->acceptProposedAction();
	}
}

void ProjectView::dropEvent(QDropEvent *event)
{
	m_ki->docManager()->openDroppedURLs(event);
}

}

#include "projectview.moc"
