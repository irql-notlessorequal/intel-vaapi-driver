%if !%{defined version}
%define version 2.4.3
%endif

# Based on https://github.com/negativo17/intel-vaapi-driver/blob/master/intel-vaapi-driver.spec
Name:       intel-vaapi-driver
Epoch:      1
Version:    %{version}
Release:    1%{?dist}
Summary:    VA-API implementation for Intel G45 and HD Graphics family (IRQL fork)
License:    MIT
URL:        https://github.com/irql-notlessorequal/%{name}

Source0:    https://github.com/irql-notlessorequal/%{name}/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
Source1:    %{name}.metainfo.xml
Source2:    %{name}.metainfo-chipsets.sh

ExclusiveArch:  %{ix86} x86_64

BuildRequires:  gcc
BuildRequires:  git
BuildRequires:  libappstream-glib >= 0.6.3
BuildRequires:  meson >= 0.43.0
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(libdrm) >= 2.4.52
BuildRequires:  pkgconfig(libva) >= 1.4.0
BuildRequires:  pkgconfig(libva-drm) >= 1.4.0
BuildRequires:  pkgconfig(libva-wayland) >= 1.1.0
BuildRequires:  pkgconfig(libva-x11) >= 1.4.0
BuildRequires:  pkgconfig(wayland-client) >= 1.11.0
BuildRequires:  pkgconfig(wayland-scanner) >= 1.11.0

Provides:       libva-intel-driver%{?_isa} = %{epoch}:%{version}-%{release}
Obsoletes:      libva-intel-driver < %{epoch}:%{version}-%{release}

%description
VA-API (Video Acceleration API) user mode driver for Intel GEN Graphics family (IRQL fork).

%prep
%autosetup -p1

%build
%meson \
  -D enable_hybrid_codec=true \
  -D with_x11=yes \
  -D with_wayland=yes

%meson_build

%install
%meson_install
find %{buildroot} -name "*.la" -delete

# Install AppData and add modalias provides
install -pm 0644 -D %{SOURCE1} %{buildroot}%{_metainfodir}/%{name}.metainfo.xml
%{SOURCE2} src/i965_pciids.h | xargs appstream-util add-provide %{buildroot}%{_metainfodir}/%{name}.metainfo.xml modalias

%check
appstream-util validate --nonet %{buildroot}%{_metainfodir}/%{name}.metainfo.xml

%files
%license LICENSE
%doc AUTHORS NEWS README
%{_libdir}/dri/i965_drv_video.so
%{_metainfodir}/%{name}.metainfo.xml
