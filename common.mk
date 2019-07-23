.RECIPEPREFIX +=
PERC := %
COMMA := ,

feval = $(eval $1)
id = $(eval __tmp := $1)$(__tmp)
proxy = $(eval $$(call $1,$2))
nofirstword = $(wordlist 2,$(words $1),$1)
nolastword = $(wordlist 2,$(words $1),0 $1)
stripfirst = $(wordlist 2,$(words 0 $1),0 $1)
striplast = $(wordlist 1,$(words $1),$1 0)

firstsep = $(call striplast,$(call __firstsep,$1,$2))
__firstsep = $(if $2,$(firstword $(subst $1, ,$(firstword $2))) $(call __firstsep,$1,$(call nofirstword,$2)))
rev = $(call stripfirst,$(call __rev,$1))
__rev = $(if $1,$(call __rev,$(call nofirstword,$1)) $(firstword $1))
rremsuffix = $(if $(filter-out $2,$(patsubst %$1,%,$2)),$(call rremsuffix,$1,$(patsubst %$1,%,$2)),$2)
rwildcard = $(call strip,$(call __rwildcard,$1,$2))
__rwildcard = $(sort $(wildcard $(addprefix $1,$2))) $(foreach d,$(sort $(wildcard $1*)),$(call __rwildcard,$d/,$2))
uniq = $(call striplast,$(call __uniq,$1))
__uniq = $(if $1,$(firstword $1) $(call __uniq,$(filter-out $(firstword $1),$1)))
inflate = $(if $1,$(call inflate,$(call nofirstword,$1),$(subst $(firstword $1),$(firstword $1) ,$2)),$(call striplast,$2))
compress = $(if $1,$(firstword $1)$(call compress,$(call nofirstword,$1)))

awrap = $(call compress,$(call $1,$(call inflate,0 1 2 3 4 5 6 7 8 9,$2),$3))
inc = $(call awrap,__inc,$1,)
__inc = $(call __inc$(lastword 0 $1),$(call nolastword,$1))
__inc0 = $1 1
__inc1 = $1 2
__inc2 = $1 3
__inc3 = $1 4
__inc4 = $1 5
__inc5 = $1 6
__inc6 = $1 7
__inc7 = $1 8
__inc8 = $1 9
__inc9 = $(call __inc,$1) 0
dec = $(call awrap,__dec,$1,0)
__dec = $(call __dec$(lastword $1),$(call nolastword,$1),$2)
__dec0 = $(call __dec,$1,) 9
__dec1 = $(if $2,$1 0)
__dec2 = $1 1
__dec3 = $1 2
__dec4 = $1 3
__dec5 = $1 4
__dec6 = $1 5
__dec7 = $1 6
__dec8 = $1 7
__dec9 = $1 8

argmsk = $(if $(filter $1,$2),,$(COMMA)$$($(call inc,$1))$(call argmsk,$(call inc,$1),$2))
argcnt = $(eval __tmp := 0)$(__argcnt)$(__tmp)
__argcnt = $(if $(filter simple,$(flavor $(call inc,$(__tmp)))),$(eval __tmp := $(call inc,$(__tmp)))$(eval $(value __argcnt)))

foreachi = $(foreach i,$($1),$(eval __tmp := $$(call $$2$(call argmsk,2,$(call dec,$1)),$$i$(call argmsk,$1,$(argcnt))))$(__tmp))
foreachl = $(eval __tmp := $$(call __foreachl,$(foreach i,$1,$(call inc,$(call inc,$i)))$(call argmsk,1,$(argcnt))))$(__tmp)
__foreachl = $(eval __tmp := $(if $1,\
$$(call foreachl,$(call nolastword,$1),foreachi,$(lastword $1)$(call argmsk,1,$(argcnt))),\
$$(call $$2$(call argmsk,2,$(argcnt)))))$(__tmp)

.PHONY: print-%
print-%:; @echo '$* = $($*)'