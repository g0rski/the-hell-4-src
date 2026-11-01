@if exist personal.props goto NEXT
@copy tools\personal.props.example personal.props
:NEXT
@if exist thehell3.vcxproj.user goto END
@copy tools\thehell3.vcxproj.user.example thehell3.vcxproj.user
:END
