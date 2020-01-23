
" Windows specific, use NMake
set makeprg=build.sh
"set makeprg=nmake\ /f\ misc\\Makefile\ /nologo

function! s:MSBuild()

	" Dont wait for key press
	"cd misc
	"silent make -f misc\Makefile -nologo
	"silent make -nologo
"	silent make
	cexpr system('build.bat')
	"cd ..

	" Id of the last window (num windows)
	let numWindows=winnr('$')

	copen

	" Create at least a vertical split
	if numWindows < 2

		" Go to quickfix and move it to far right
		wincmd L

	else

		" Move quickfix window to top if at bottom
		if winnr() == winnr('$')
			wincmd x
		endif

	endif

endfunction

function! s:CompileShaders()

	" Dont wait for key press
	"cd misc
	"silent make -f misc\Makefile -nologo
	"silent make -nologo
"	silent make
	cexpr system('compile_shaders.bat')
	"cd ..

	" Id of the last window (num windows)
	let numWindows=winnr('$')

	copen

	" Create at least a vertical split
	if numWindows < 2

		" Go to quickfix and move it to far right
		wincmd L

	else

		" Move quickfix window to top if at bottom
		if winnr() == winnr('$')
			wincmd x
		endif

	endif

endfunction

function! s:MSDebug()
	cexpr system('debug.bat')
endfunction

" Custom mappings
nnoremap <leader>b :call <SID>MSBuild()<cr>

" Build solution on <F7>
nnoremap <F7> :call <SID>MSBuild()<cr>

" Debug binary
nnoremap <F5> :call <SID>MSDebug()<cr>

" Compile shaders
nnoremap <F8> :call <SID>CompileShaders()<cr>

