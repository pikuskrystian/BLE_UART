#include "bledevice.h"

BLEDevice::BLEDevice(QObject *parent) : QObject(parent),
    currentDevice(QBluetoothDeviceInfo()),
    controller(0),
    service(0)
{
    DiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    DiscoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    connect(DiscoveryAgent, SIGNAL(deviceDiscovered(const QBluetoothDeviceInfo&)), this, SLOT(addDevice(const QBluetoothDeviceInfo&)));
    connect(DiscoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)), this, SLOT(deviceScanError(QBluetoothDeviceDiscoveryAgent::Error)));
    connect(DiscoveryAgent, SIGNAL(finished()), this, SLOT(scanFinished()));
}

BLEDevice::~BLEDevice()
{
    delete DiscoveryAgent;
    delete controller;
}

QStringList BLEDevice::deviceListModel()
{
    return m_deviceListModel;
}

void BLEDevice::setDeviceListModel(QStringList deviceListModel)
{
    if (m_deviceListModel == deviceListModel)
        return;

    m_deviceListModel = deviceListModel;
    emit deviceListModelChanged(m_deviceListModel);
}

void BLEDevice::resetDeviceListModel()
{
    m_deviceListModel.clear();
}

void BLEDevice::addDevice(const QBluetoothDeviceInfo &device)
{
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        qDebug()<<"Discovered Device:"<<device.name()<<"Address: "<<device.address().toString()<<"RSSI:"<< device.rssi()<<"dBm";

        if(!m_foundDevices.contains(device.name(), Qt::CaseSensitive) && device.name().size()) {
            m_foundDevices.append(device.name());

            DeviceInfo *dev = new DeviceInfo(device);
            qlDevices.append(dev);
        }
    }
}

void BLEDevice::scanFinished()
{
    setDeviceListModel(m_foundDevices);
    emit scanningFinished();
}

void BLEDevice::deviceScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError)
        qDebug() << "The Bluetooth adaptor is powered off.";
    else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError)
        qDebug() << "Writing or reading from the device resulted in an error.";
    else
        qDebug() << "An unknown error has occurred.";
}

void BLEDevice::startScan()
{
    qDeleteAll(qlDevices);
    qlDevices.clear();
    m_foundDevices.clear();
    resetDeviceListModel();
    DiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    qDebug()<< "Searching for BLE devices..." ;
}

void BLEDevice::startConnect(int i)
{
    currentDevice.setDevice(((DeviceInfo*)qlDevices.at(i))->getDevice());
    if (controller) {
        controller->disconnectFromDevice();
        delete controller;
        controller = 0;

    }

    controller = new QLowEnergyController(currentDevice.getDevice(), this);
    controller ->setRemoteAddressType(QLowEnergyController::RandomAddress);

    connect(controller, SIGNAL(serviceDiscovered(QBluetoothUuid)), this, SLOT(serviceDiscovered(QBluetoothUuid)));
    connect(controller, SIGNAL(discoveryFinished()), this, SLOT(serviceScanDone()));
    connect(controller, SIGNAL(error(QLowEnergyController::Error)),  this, SLOT(controllerError(QLowEnergyController::Error)));
    connect(controller, SIGNAL(connected()), this, SLOT(deviceConnected()));
    connect(controller, SIGNAL(disconnected()), this, SLOT(deviceDisconnected()));

    controller->connectToDevice();
}

void BLEDevice::serviceDiscovered(const QBluetoothUuid &gatt)
{
    if(gatt==QBluetoothUuid(QUuid(UARTSERVICEUUID))) {
        bFoundUARTService =true;
        qDebug() << "UART service found";
    }
}

void BLEDevice::serviceScanDone()
{
    delete service;
    service=0;

    if(bFoundUARTService) {
        qDebug() << "Connecting to UART service...";
        service = controller->createServiceObject(QBluetoothUuid(QUuid(UARTSERVICEUUID)),this);
    }

    if(!service) {
        qDebug() <<"UART service not found";
        return;
    }

    connect(service, SIGNAL(stateChanged(QLowEnergyService::ServiceState)),this, SLOT(serviceStateChanged(QLowEnergyService::ServiceState)));
    connect(service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)),this, SLOT(updateData(QLowEnergyCharacteristic, QByteArray)));
    connect(service, SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)),this, SLOT(confirmedDescriptorWrite(QLowEnergyDescriptor, QByteArray)));

    service->discoverDetails();
}

void BLEDevice::deviceDisconnected()
{
    qDebug() << "Remote device disconnected";
}

void BLEDevice::deviceConnected()
{
    qDebug() << "Device connected";
    controller->discoverServices();
}

void BLEDevice::controllerError(QLowEnergyController::Error error)
{
    qDebug() << "Controller Error:" << error;
}

void BLEDevice::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::ServiceDiscovered:
    {
        //TX characteristic
        const QLowEnergyCharacteristic TxChar = service->characteristic(QBluetoothUuid(QUuid(TXUUID)));
        if (!TxChar.isValid()){
            qDebug() << "Tx characteristic not found";
            break;
        }

        //RX characteristic
        const QLowEnergyCharacteristic  RxChar = service->characteristic(QBluetoothUuid(QUuid(RXUUID)));
        if (!RxChar.isValid()) {
            qDebug() << "Rx characteristic not found";
            break;
        }

        // Rx notify enabled
        const QLowEnergyDescriptor m_notificationDescRx = RxChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
        if (m_notificationDescRx.isValid()) {
            // enable notification
            service->writeDescriptor(m_notificationDescRx, QByteArray::fromHex("0100"));
            qDebug() << "Notification enabled";
            emit connectionStart();
        }
        break;
    }
    default:

        break;
    }
}

void BLEDevice::confirmedDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value)
{
    if (d.isValid() && d == notificationDesc && value == QByteArray("0000"))
    {
        controller->disconnectFromDevice();
        delete service;
        service = nullptr;
    }
}

void BLEDevice::writeData(QByteArray v)
{
    const QLowEnergyCharacteristic  TxChar = service->characteristic(QBluetoothUuid(QUuid(TXUUID)));
    service->writeCharacteristic(TxChar, v, QLowEnergyService::WriteWithoutResponse);
}

void BLEDevice::updateData(const QLowEnergyCharacteristic &c, const QByteArray &v)
{
    if (c.uuid() != QBluetoothUuid(QUuid(RXUUID)))
        return;
    qDebug()<<v;
    emit newData(v);
}
