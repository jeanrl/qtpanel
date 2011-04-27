#include "dockapplet.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneMouseEvent>
#include "textgraphicsitem.h"
#include "panelapplication.h"
#include "panelwindow.h"
#include "x11support.h"
#include "animationutils.h"

DockItem::DockItem(DockApplet* dockApplet)
{
	m_dockApplet = dockApplet;

	m_animationTimer = new QTimer();

	m_animationTimer->setInterval(20);
	m_animationTimer->setSingleShot(true);
	connect(m_animationTimer, SIGNAL(timeout()), this, SLOT(animate()));

	setParentItem(m_dockApplet);
	setAcceptsHoverEvents(true);
	setAcceptedMouseButtons(Qt::LeftButton);

	m_textItem = new TextGraphicsItem(this);
	m_textItem->setColor(Qt::white);
	m_textItem->setFont(m_dockApplet->panelWindow()->font());

	m_iconItem = new QGraphicsPixmapItem(this);

	m_dockApplet->registerDockItem(this);
}

DockItem::~DockItem()
{
	delete m_iconItem;
	delete m_textItem;
	delete m_animationTimer;

	m_dockApplet->unregisterDockItem(this);
}

void DockItem::updateContent()
{
	if(m_clients.isEmpty())
		return;

	QFontMetrics fontMetrics(m_textItem->font());
	QString shortName = fontMetrics.elidedText(m_clients[0]->name(), Qt::ElideRight, m_targetSize.width() - 36);
	m_textItem->setText(shortName);
	m_textItem->setPos(28.0, m_dockApplet->panelWindow()->textBaseLine());

	m_iconItem->setPixmap(m_clients[0]->icon().pixmap(16));
	m_iconItem->setPos(8.0, m_targetSize.height()/2 - 8);

	update();
}

void DockItem::addClient(Client* client)
{
	m_clients.append(client);
	updateClientsIconGeometry();
	updateContent();
}

void DockItem::removeClient(Client* client)
{
	m_clients.remove(m_clients.indexOf(client));
	if(m_clients.isEmpty())
	{
		// TODO: Stub. Item may be a launcher.
		delete this;
	}
	else
	{
		updateContent();
	}
}

void DockItem::setTargetPosition(const QPoint& targetPosition)
{
	m_targetPosition = targetPosition;
	updateClientsIconGeometry();
}

void DockItem::setTargetSize(const QSize& targetSize)
{
	m_targetSize = targetSize;
	updateClientsIconGeometry();
	updateContent();
}

void DockItem::moveInstantly()
{
	m_position = m_targetPosition;
	m_size = m_targetSize;
	setPos(m_position.x(), m_position.y());
	update();
}

void DockItem::startAnimation()
{
	if(!m_animationTimer->isActive())
		m_animationTimer->start();
}

void DockItem::animate()
{
	bool needAnotherStep = false;

	static const qreal highlightAnimationSpeed = 0.15;
	qreal targetIntensity = isUnderMouse() ? 1.0 : 0.0;
	m_highlightIntensity = AnimationUtils::animate(m_highlightIntensity, targetIntensity, highlightAnimationSpeed, needAnotherStep);

	static const int positionAnimationSpeed = 24;
	static const int sizeAnimationSpeed = 24;
	m_position.setX(AnimationUtils::animate(m_position.x(), m_targetPosition.x(), positionAnimationSpeed, needAnotherStep));
	m_position.setY(AnimationUtils::animate(m_position.y(), m_targetPosition.y(), positionAnimationSpeed, needAnotherStep));
	m_size.setWidth(AnimationUtils::animate(m_size.width(), m_targetSize.width(), sizeAnimationSpeed, needAnotherStep));
	m_size.setHeight(AnimationUtils::animate(m_size.height(), m_targetSize.height(), sizeAnimationSpeed, needAnotherStep));
	setPos(m_position.x(), m_position.y());

	update();

	if(needAnotherStep)
		m_animationTimer->start();
}

QRectF DockItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void DockItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->setPen(Qt::NoPen);
	QPointF center(m_size.width()/2.0, m_size.height() + 32.0);
	QRadialGradient gradient(center, 200.0, center);
	gradient.setColorAt(0.0, QColor(255, 255, 255, 80 + static_cast<int>(80*m_highlightIntensity)));
	gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
	painter->setBrush(QBrush(gradient));
	painter->drawRoundedRect(QRectF(0.0, 4.0, m_size.width(), m_size.height() - 8.0), 3.0, 3.0);
}

void DockItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	startAnimation();
}

void DockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	startAnimation();
}

void DockItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
}

void DockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if(isUnderMouse())
	{
		if(m_clients.isEmpty())
			return;

		if(m_dockApplet->activeWindow() == m_clients[0]->handle())
			X11Support::instance()->minimizeWindow(m_clients[0]->handle());
		else
			X11Support::instance()->activateWindow(m_clients[0]->handle());
	}
}

void DockItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
}

void DockItem::updateClientsIconGeometry()
{
	QPointF topLeft = mapToScene(boundingRect().topLeft());
	QVector<unsigned long> values;
	values.resize(4);
	values[0] = static_cast<unsigned long>(topLeft.x()) + m_dockApplet->panelWindow()->pos().x();
	values[1] = static_cast<unsigned long>(topLeft.y()) + m_dockApplet->panelWindow()->pos().y();
	values[2] = m_size.width();
	values[3] = m_size.height();
	for(int i = 0; i < m_clients.size(); i++)
	{
		X11Support::instance()->setWindowPropertyCardinalArray(m_clients[i]->handle(), "_NET_WM_ICON_GEOMETRY", values);
	}
}

Client::Client(DockApplet* dockApplet, unsigned long handle)
	: m_dockItem(NULL)
{
	m_dockApplet = dockApplet;
	m_handle = handle;

	X11Support::instance()->registerForWindowPropertyChanges(m_handle);

	updateVisibility();
	updateName();
	updateIcon();
}

Client::~Client()
{
	if(m_dockItem != NULL)
	{
		m_dockItem->removeClient(this);
	}
}

void Client::updateVisibility()
{
	QVector<unsigned long> windowTypes = X11Support::instance()->getWindowPropertyAtomsArray(m_handle, "_NET_WM_WINDOW_TYPE");
	QVector<unsigned long> windowStates = X11Support::instance()->getWindowPropertyAtomsArray(m_handle, "_NET_WM_STATE");

	// Show only regular windows in dock.
	// When no window type is set, assume it's normal window.
	m_visible = (windowTypes.size() == 0) || (windowTypes.size() == 1 && windowTypes[0] == X11Support::instance()->atom("_NET_WM_WINDOW_TYPE_NORMAL"));
	// Don't show window if requested explicitly in window states.
	if(windowStates.contains(X11Support::instance()->atom("_NET_WM_STATE_SKIP_TASKBAR")))
		m_visible = false;

	if(m_dockItem == NULL && m_visible)
	{
		m_dockItem = m_dockApplet->dockItemForClient(this);
		m_dockItem->addClient(this);
	}

	if(m_dockItem != NULL && !m_visible)
	{
		m_dockItem->removeClient(this);
		m_dockItem = NULL;
	}
}

void Client::updateName()
{
	m_name = X11Support::instance()->getWindowName(m_handle);
	if(m_dockItem != NULL)
		m_dockItem->updateContent();
}

void Client::updateIcon()
{
	m_icon = X11Support::instance()->getWindowIcon(m_handle);
	if(m_dockItem != NULL)
		m_dockItem->updateContent();
}

DockApplet::DockApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
	// Register for notifications about window property changes.
	connect(PanelApplication::instance(), SIGNAL(windowPropertyChanged(ulong,ulong)), this, SLOT(windowPropertyChanged(ulong,ulong)));
}

DockApplet::~DockApplet()
{
	while(!m_clients.isEmpty())
	{
		unsigned long key = m_clients.begin().key();
		delete m_clients.begin().value();
		m_clients.remove(key);
	}

	while(!m_dockItems.isEmpty())
	{
		delete m_dockItems[m_dockItems.size() - 1];
	}
}

bool DockApplet::init()
{
	updateClientList();
	updateActiveWindow();

	return true;
}

void DockApplet::updateLayout()
{
	// TODO: Vertical orientation support.

	int freeSpace = m_size.width() - 8;
	int spaceForOneClient = (m_dockItems.size() > 0) ? freeSpace/m_dockItems.size() : 0;
	int currentPosition = 4;
	for(int i = 0; i < m_dockItems.size(); i++)
	{
		int spaceForThisClient = spaceForOneClient;
		if(spaceForThisClient > 256)
			spaceForThisClient = 256;
		m_dockItems[i]->setTargetPosition(QPoint(currentPosition, 0));
		m_dockItems[i]->setTargetSize(QSize(spaceForThisClient - 4, m_size.height()));
		m_dockItems[i]->startAnimation();
		currentPosition += spaceForThisClient;
	}

	update();
}

void DockApplet::layoutChanged()
{
	updateLayout();
}

QSize DockApplet::desiredSize()
{
	return QSize(-1, -1); // Take all available space.
}

void DockApplet::registerDockItem(DockItem* dockItem)
{
	m_dockItems.append(dockItem);
	updateLayout();
	dockItem->moveInstantly();
}

void DockApplet::unregisterDockItem(DockItem* dockItem)
{
	m_dockItems.remove(m_dockItems.indexOf(dockItem));
	updateLayout();
}

DockItem* DockApplet::dockItemForClient(Client* client)
{
	// FIXME: Stub.
	return new DockItem(this);
}

void DockApplet::updateClientList()
{
	QVector<unsigned long> windows = X11Support::instance()->getWindowPropertyWindowsArray(X11Support::instance()->rootWindow(), "_NET_CLIENT_LIST");

	// Handle new clients.
	for(int i = 0; i < windows.size(); i++)
	{
		if(!m_clients.contains(windows[i]))
		{
			// Skip our own windows.
			if(QWidget::find(windows[i]) != NULL)
				continue;

			m_clients[windows[i]] = new Client(this, windows[i]);
		}
	}

	// Handle removed clients.
	for(;;)
	{
		bool clientRemoved = false;
		foreach(Client* client, m_clients)
		{
			int handle = client->handle();
			if(!windows.contains(handle))
			{
				delete m_clients[handle];
				m_clients.remove(handle);
				clientRemoved = true;
				break;
			}
		}
		if(!clientRemoved)
			break;
	}
}

void DockApplet::updateActiveWindow()
{
	m_activeWindow = X11Support::instance()->getWindowPropertyWindow(X11Support::instance()->rootWindow(), "_NET_ACTIVE_WINDOW");
}

void DockApplet::windowPropertyChanged(unsigned long window, unsigned long atom)
{
	if(window == X11Support::instance()->rootWindow())
	{
		if(atom == X11Support::instance()->atom("_NET_CLIENT_LIST"))
		{
			updateClientList();
		}

		if(atom == X11Support::instance()->atom("_NET_ACTIVE_WINDOW"))
		{
			updateActiveWindow();
		}

		return;
	}

	if(!m_clients.contains(window))
		return;

	if(atom == X11Support::instance()->atom("_NET_WM_WINDOW_TYPE") || atom == X11Support::instance()->atom("_NET_WM_STATE"))
	{
		m_clients[window]->updateVisibility();
	}

	if(atom == X11Support::instance()->atom("_NET_WM_VISIBLE_NAME") || atom == X11Support::instance()->atom("_NET_WM_NAME") || atom == X11Support::instance()->atom("WM_NAME"))
	{
		m_clients[window]->updateName();
	}

	if(atom == X11Support::instance()->atom("_NET_WM_ICON"))
	{
		m_clients[window]->updateIcon();
	}
}
