%bcond_with wayland

Name: libwidget_viewer
Summary: Library for developing the application
Version: 1.2.2
Release: 1
Group: Applications/Core Applications
License: Flora-1.1
Source0: %{name}-%{version}.tar.gz
Source1001: %{name}.manifest
Source1002: org.tizen.widget_viewer_sdk.manifest
BuildRequires: cmake, gettext-tools, coreutils, edje-bin
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(com-core)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(db-util)
BuildRequires: pkgconfig(widget_service)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(capi-appfw-application)
BuildRequires: pkgconfig(appcore-efl)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(efl-extension)
BuildRequires: pkgconfig(security-privilege-checker)
#BuildRequires: pkgconfig(dali)
#BuildRequires: pkgconfig(dali-core)
#BuildRequires: pkgconfig(dali-toolkit)

%if %{with wayland}
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(libtbm)
%else
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xext)
BuildRequires: pkgconfig(ecore-x)
BuildRequires: pkgconfig(ecore)
%endif

%description
API for creating a new instance of the widget and managing its life-cycle.

%package devel
Summary: Development Library for widget Viewer Application (dev)
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Header and package configuration files for the widget viewer development

%prep
%setup -q
cp %{SOURCE1001} .
cp %{SOURCE1002} .

%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"

export CFLAGS="${CFLAGS} -DTIZEN_ENGINEER_MODE"
export CXXFLAGS="${CXXFLAGS} -DTIZEN_ENGINEER_MODE"
export FFLAGS="${FFLAGS} -DTIZEN_ENGINEER_MODE"

%if %{with wayland}
export WAYLAND_SUPPORT=On
export X11_SUPPORT=Off
%else
export WAYLAND_SUPPORT=Off
export X11_SUPPORT=On
%endif

%cmake . -DWAYLAND_SUPPORT=${WAYLAND_SUPPORT} -DX11_SUPPORT=${X11_SUPPORT} -DWIDGET_ENABLED=On
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%post -n %{name} -p /sbin/ldconfig
%postun -n %{name} -p /sbin/ldconfig

%files -n %{name}
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/%{name}.so*
%{_datarootdir}/license/%{name}

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_includedir}/widget_viewer/widget_viewer.h
%{_libdir}/pkgconfig/widget_viewer.pc

#################################################
## libwidget_viewer_dali
#%package -n %{name}_dali
#Summary: Library for developing the widget viewer dali
#Group: Applications/Core Applications
#License: Flora-1.1
#
#%description -n %{name}_dali
#Provider APIs to develop the widget viewer Dali application.
#
#%package -n %{name}_dali-devel
#Summary: Widget viewer application development library (dev) (Dali version)
#Group: Development/Libraries
#Requires: %{name}_dali
#
#%description -n %{name}_dali-devel
#Header & package configuration files to support development of the widget viewer applications. (for Dali app)
#
#%post -n %{name}_dali -p /sbin/ldconfig
#%postun -n %{name}_dali -p /sbin/ldconfig
#
#################################################
# libwidget_viewer_evas
%package -n %{name}_evas
Summary: Library for developing the widget viewer evas
Group: Applications/Core Applications
License: Flora-1.1

%description -n %{name}_evas
Provider APIs to develop the widget viewer EFL application.

%package -n %{name}_evas-devel
Summary: Widget provider application development library (dev) (EFL version)
Group: Development/Libraries
Requires: %{name}_evas

%description -n %{name}_evas-devel
Header & package configuration files to support development of the widget viewer applications. (for EFL app)

%post -n %{name}_evas -p /sbin/ldconfig
%postun -n %{name}_evas -p /sbin/ldconfig

#################################################
# org.tizen.widget_viewer_sdk
%package -n org.tizen.widget_viewer_sdk
Summary: The widget viewer for development using SDK(IDE)
Version: 0.0.1
Group: Development/Tools
License: Flora-1.1
Requires: %{name}_evas

%description -n org.tizen.widget_viewer_sdk
While developing the widget applications, this viewer will load it and execute it to help you to see it on the screen.

%post -n org.tizen.widget_viewer_sdk -p /sbin/ldconfig
%postun -n org.tizen.widget_viewer_sdk -p /sbin/ldconfig

#%files -n %{name}_dali
#%manifest %{name}.manifest
#%defattr(-,root,root,-)
#%{_libdir}/%{name}_dali.so*
#%{_datadir}/widget_viewer_dali/*
#%{_datarootdir}/license/%{name}_dali
#
#%files -n %{name}_dali-devel
#%manifest %{name}.manifest
#%defattr(-,root,root,-)
#%{_includedir}/widget_viewer_dali/widget_viewer_dali.h
#%{_includedir}/widget_viewer_dali/widget_dali.h
#%{_includedir}/widget_viewer_dali/widget_viewer_dali_generic.h
#%{_libdir}/pkgconfig/widget_viewer_dali.pc

%files -n %{name}_evas
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/%{name}_evas.so*
%{_datadir}/widget_viewer_evas/*
%{_datarootdir}/license/%{name}_evas

%files -n %{name}_evas-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_includedir}/widget_viewer_evas/widget_viewer_evas.h
%{_includedir}/widget_viewer_evas/widget_viewer_evas_internal.h
%{_libdir}/pkgconfig/widget_viewer_evas.pc

%files -n org.tizen.widget_viewer_sdk
%manifest org.tizen.widget_viewer_sdk.manifest
%defattr(-,root,root,-)
%attr(-,app,app) %dir /opt/usr/apps/org.tizen.widget_viewer_sdk/data
%{_datarootdir}/packages/org.tizen.widget_viewer_sdk.xml
%{_sysconfdir}/smack/accesses.d
%{_datarootdir}/license/org.tizen.widget_viewer_sdk
%{_prefix}/apps/org.tizen.widget_viewer_sdk/*

# End of a file
