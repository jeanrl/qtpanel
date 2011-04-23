#ifndef DOCKAPPLET_H
#define DOCKAPPLET_H

#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtGui/QIcon>
#include <QtGui/QGraphicsItem>
#include "applet.h"

class QGraphicsPixmapItem;
class TextGraphicsItem;
class DockApplet;
class Client;

// Represents a single item in a dock.
// There isn't one to one relationship between window (client) and dock item, that's why
// it's separate entity. One dock item can represent pinned launcher and one or more opened
// windows of that application.
class DockItem: public QGraphicsItem
{
public:
	DockItem(DockApplet* dockApplet);
	~DockItem();

	void updateContent();

	void addClient(Client* client);
	void removeClient(Client* client);

	void setPosition(const QPoint& position);
	void setSize(const QSize& size);

	const QVector<Client*>& clients() const
	{
		return m_clients;
	}

	QRectF boundingRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

private:
	DockApplet* m_dockApplet;
	TextGraphicsItem* m_textItem;
	QGraphicsPixmapItem* m_iconItem;
	QVector<Client*> m_clients;
	QSize m_size;
};

// Used for tracking connected windows (X11 clients).
// Client may have it's DockItem, but not necessary (for example, special windows are not shown in dock).
class Client
{
public:
	Client(DockApplet* dockApplet, unsigned long handle);
	~Client();

	unsigned long handle() const
	{
		return m_handle;
	}

	bool isVisible()
	{
		return m_visible;
	}

	const QString& name() const
	{
		return m_name;
	}

	const QIcon& icon() const
	{
		return m_icon;
	}

	void updateVisibility();
	void updateName();
	void updateIcon();

private:
	DockApplet* m_dockApplet;
	unsigned long m_handle;
	QString m_name;
	QIcon m_icon;
	bool m_visible;
	DockItem* m_dockItem;
};

class DockApplet: public Applet
{
	Q_OBJECT
public:
	DockApplet(PanelWindow* panelWindow);
	~DockApplet();

	bool init();
	QSize desiredSize();

	void registerDockItem(DockItem* dockItem);
	void unregisterDockItem(DockItem* dockItem);

	DockItem* dockItemForClient(Client* client);

	void updateLayout();

protected:
	void layoutChanged();

private slots:
	void windowPropertyChanged(unsigned long window, unsigned long atom);

private:
	void clientListChanged();

	QMap<unsigned long, Client*> m_clients;
	QVector<DockItem*> m_dockItems;
};

#endif
