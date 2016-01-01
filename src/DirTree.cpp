/*
 *   File name: DirTree.cpp
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QDir>
#include <QFileInfo>

#include "DirTree.h"
#include "Exception.h"
#include "DirTreeCache.h"

using namespace QDirStat;



DirTree::DirTree()
    : QObject()
{
    _isBusy    = false;
    _root      = new DirInfo( this );
    CHECK_NEW( _root );

    readConfig();

    connect( & _jobQueue, SIGNAL( finished()	 ),
	     this,	  SLOT	( slotFinished() ) );
}


DirTree::~DirTree()
{
    if ( _root )
	delete _root;
}


void DirTree::readConfig()
{
#if 0
    // FIXME
    // FIXME
    KConfig * config = kapp->config();
    config->setGroup( "Directory Reading" );

    _crossFileSystems	  = config->readBoolEntry( "CrossFileSystems",	   false );
    _enableLocalDirReader = config->readBoolEntry( "EnableLocalDirReader", true	 );
    // FIXME
    // FIXME
#else
    _crossFileSystems	  = false;
    _enableLocalDirReader = true;
#endif
}


void DirTree::setRoot( DirInfo *newRoot )
{
    if ( _root )
    {
	emit deletingChild( _root );
	delete _root;
	emit childDeleted();
    }

    _root = newRoot;
}


FileInfo * DirTree::firstToplevel() const
{
    return _root ? _root->firstChild() : 0;
}


bool DirTree::isTopLevel( FileInfo *item ) const
{
    return item && item->parent() && ! item->parent()->parent();
}


QString DirTree::url() const
{
    FileInfo * realRoot = firstToplevel();

    return realRoot ? realRoot->url() : "";
}


void DirTree::clear()
{
    _jobQueue.clear();

    if ( _root )
    {
        emit clearing();
	_root->clear();
    }

    _isBusy = false;
}


void DirTree::startReading( const QString & rawUrl )
{
    QFileInfo fileInfo( rawUrl );
    QString url = fileInfo.absoluteFilePath();
    // logDebug() << "rawUrl: \"" << rawUrl << "\"" << endl;
    logDebug() << "   url: \"" << url	 << "\"" << endl;

    _isBusy = true;

    if ( _root->hasChildren() )
	clear();
    emit startingReading();
    readConfig();

    FileInfo * item = LocalDirReadJob::stat( url, this, _root );
    CHECK_PTR( item );

    if ( item )
    {
	childAddedNotify( item );

	if ( item->isDirInfo() )
	{
	    addJob( new LocalDirReadJob( this, item->toDirInfo() ) );
	    emit readJobFinished( _root );
	}
	else
	{
	    _isBusy = false;
	    emit readJobFinished( _root );
	    emit finished();
	}
    }
    else	// stat() failed
    {
	logWarning() << "stat(" << url << ") failed" << endl;
	_isBusy = false;
	emit finished();
	emit finalizeLocal( 0 );
    }
}


void DirTree::refresh( FileInfo *subtree )
{
    if ( ! _root )
	return;

    if ( ! subtree || ! subtree->parent() )	// Refresh all (from root)
    {
	startReading( QDir::cleanPath( firstToplevel()->url() ) );
    }
    else	// Refresh subtree
    {
	// Save some values from the old subtree.

	QString	  url	 = subtree->url();
	DirInfo * parent = subtree->parent();


	// Clear any old "excluded" status

	subtree->setExcluded( false );


	// Get rid of the old subtree.

	emit deletingChild( subtree );

	// logDebug() << "Deleting subtree " << subtree << endl;

	/**
	 * This may sound stupid, but the parent must be told to unlink its
	 * child from the children list. The child cannot simply do this by
	 * itself in its destructor since at this point important parts of the
	 * object may already be destroyed, e.g., the virtual table -
	 * i.e. virtual methods won't work any more.
	 *
	 * I just found that out the hard way by several hours of debugging. ;-}
	 **/
	parent->deletingChild( subtree );
	delete subtree;
	emit childDeleted();

	_isBusy = true;
	emit startingReading();

	// Create new subtree root.

	subtree = LocalDirReadJob::stat( url, this, parent );

	// logDebug() << "New subtree: " << subtree << endl;

	if ( subtree )
	{
	    // Insert new subtree root into the tree hierarchy.

	    parent->insertChild( subtree );
	    childAddedNotify( subtree );

	    if ( subtree->isDirInfo() )
	    {
		// Prepare reading this subtree's contents.
		addJob( new LocalDirReadJob( this, subtree->toDirInfo() ) );
	    }
	    else
	    {
		_isBusy = false;
		emit finished();
	    }
	}
    }
}


void DirTree::abortReading()
{
    if ( _jobQueue.isEmpty() )
	return;

    _jobQueue.abort();

    _isBusy = false;
    emit aborted();
}


void DirTree::slotFinished()
{
    _isBusy = false;
    emit finished();
}


void DirTree::childAddedNotify( FileInfo *newChild )
{
    emit childAdded( newChild );

    if ( newChild->dotEntry() )
	emit childAdded( newChild->dotEntry() );
}


void DirTree::deletingChildNotify( FileInfo *deletedChild )
{
    logDebug() << "Deleting child " << deletedChild << endl;
    emit deletingChild( deletedChild );

    if ( deletedChild == _root )
	_root = 0;
}


void DirTree::childDeletedNotify()
{
    emit childDeleted();
}


void DirTree::deleteSubtree( FileInfo *subtree )
{
    // logDebug() << "Deleting subtree " << subtree << endl;
    DirInfo * parent = subtree->parent();

    // Send notification to anybody interested (e.g., to attached views)
    deletingChildNotify( subtree );

    if ( parent )
    {
	if ( parent->isDotEntry() && ! parent->hasChildren() )
	    // This was the last child of a dot entry
	{
	    // Get rid of that now empty and useless dot entry

	    if ( parent->parent() )
	    {
		if ( parent->parent()->isFinished() )
		{
		    // logDebug() << "Removing empty dot entry " << parent << endl;

		    deletingChildNotify( parent );
		    parent->parent()->setDotEntry( 0 );

		    delete parent;
		}
	    }
	    else	// no parent - this should never happen (?)
	    {
		logError() << "Internal error: Killing dot entry without parent " << parent << endl;

		// Better leave that dot entry alone - we shouldn't have come
		// here in the first place. Who knows what will happen if this
		// thing is deleted now?!
		//
		// Intentionally NOT calling:
		//     delete parent;
	    }
	}
    }

    if ( parent )
    {
	// Give the parent of the child to be deleted a chance to unlink the
	// child from its children list and take care of internal summary
	// fields
	parent->deletingChild( subtree );
    }

    delete subtree;

    if ( subtree == _root )
    {
	_root = 0;
    }

    emit childDeleted();
}


void DirTree::addJob( DirReadJob * job )
{
    _jobQueue.enqueue( job );
}


void DirTree::sendProgressInfo( const QString &infoLine )
{
    emit progressInfo( infoLine );
}


void DirTree::sendFinalizeLocal( DirInfo *dir )
{
    emit finalizeLocal( dir );
}


void DirTree::sendStartingReading()
{
    emit startingReading();
}


void DirTree::sendFinished()
{
    emit finished();
}


void DirTree::sendAborted()
{
    emit aborted();
}


void DirTree::sendStartingReading( DirInfo * dir )
{
    emit startingReading( dir );
}


void DirTree::sendReadJobFinished( DirInfo * dir )
{
    // logDebug() << dir << endl;
    emit readJobFinished( dir );
}


bool DirTree::writeCache( const QString & cacheFileName )
{
    CacheWriter writer( cacheFileName.toUtf8(), this );
    return writer.ok();
}


void DirTree::readCache( const QString & cacheFileName )
{
    _isBusy = true;
    emit startingReading();
    addJob( new CacheReadJob( this, 0, cacheFileName ) );
}



// EOF
