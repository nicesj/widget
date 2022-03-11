%bcond_with wayland

Name: libwidget_provider
Summary: Library for developing the widget service provider
Version: 1.1.5
Release: 1
Group: Applications/Core Applications
License: Flora-1.1
Source0: %{name}-%{version}.tar.gz
Source1001: %{name}.manifest

BuildRequires: cmake, gettext-tools, coreutils
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(com-core)
BuildRequires: pkgconfig(widget_service)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(eina)
BuildRequires: pkgconfig(capi-appfw-application)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(libtbm)

%if %{with wayland}
%else
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xext)
BuildRequires: pkgconfig(libdri2)
BuildRequires: pkgconfig(xfixes)
BuildRequires: pkgconfig(dri2proto)
BuildRequires: pkgconfig(xdamage)
%endif

%description
Supporting the commnuncation channel with master service for widget remote view.
API for accessing the remote buffer of widgetes.
Feature for life-cycle management by the master provider.

%package devel
Summary: Header & package configuration files
Group: Development/Libraries
License: Flora-1.1
Requires: %{name} = %{version}-%{release}

%description devel
Widget data provider development library (dev)

%prep
%setup -q
cp %{SOURCE1001} .

%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

%if 0%{?tizen_build_binary_release_type_eng}
export CFLAGS="${CFLAGS} -DTIZEN_ENGINEER_MODE"
export CXXFLAGS="${CXXFLAGS} -DTIZEN_ENGINEER_MODE"
export FFLAGS="${FFLAGS} -DTIZEN_ENGINEER_MODE"
%endif

%if %{with wayland}
export WAYLAND_SUPPORT=On
export X11_SUPPORT=Off
%else
export WAYLAND_SUPPORT=Off
export X11_SUPPORT=On
%endif

%cmake . -DWAYLAND_SUPPORT=${WAYLAND_SUPPORT} -DX11_SUPPORT=${X11_SUPPORT}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/%{_datarootdir}/license

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
%{_includedir}/widget_provider/widget_provider.h
%{_includedir}/widget_provider/widget_provider_buffer.h
%{_libdir}/pkgconfig/widget_provider.pc


#################################################
# libwidget_provider_app
%package -n %{name}_app
Summary: Library for developing the widget app provider
Group: Applications/Core Applications
License: Flora-1.1

%description -n %{name}_app
Provider APIs to develop the widget provider applications.

%package -n %{name}_app-devel
Summary: Widget provider application development library (dev)
Group: Development/Libraries
License: Flora-1.1
Requires: %{name}_app

%description -n %{name}_app-devel
Header & package configuration files to support development of the widget provider applications.

%post -n %{name}_app -p /sbin/ldconfig
%postun -n %{name}_app -p /sbin/ldconfig

%files -n %{name}_app
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/%{name}_app.so*
%{_datarootdir}/license/%{name}_app

%files -n %{name}_app-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_includedir}/widget_provider_app/widget_provider_app.h
%{_includedir}/widget_provider_app/widget_provider_app_internal.h
%{_libdir}/pkgconfig/widget_provider_app.pc

# End of a file
